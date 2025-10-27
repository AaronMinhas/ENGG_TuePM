#include "TrafficCounter.h"

#include "Logger.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

#ifndef UNIT_TEST
namespace {
    portMUX_TYPE s_isrMux = portMUX_INITIALIZER_UNLOCKED;
}
#endif

TrafficCounter* TrafficCounter::s_instance = nullptr;

TrafficCounter::TrafficCounter(EventBus& bus)
    : bus_(bus),
      leftCount_(0),
      rightCount_(0) {
    buttons_[static_cast<uint8_t>(ButtonId::LeftEntry)] = {
        DEFAULT_LEFT_ENTRY_PIN, TrafficSide::Left, ButtonRole::Entry, false, 0};
    buttons_[static_cast<uint8_t>(ButtonId::LeftExit)] = {
        DEFAULT_LEFT_EXIT_PIN, TrafficSide::Left, ButtonRole::Exit, false, 0};
    buttons_[static_cast<uint8_t>(ButtonId::RightEntry)] = {
        DEFAULT_RIGHT_ENTRY_PIN, TrafficSide::Right, ButtonRole::Entry, false, 0};
    buttons_[static_cast<uint8_t>(ButtonId::RightExit)] = {
        DEFAULT_RIGHT_EXIT_PIN, TrafficSide::Right, ButtonRole::Exit, false, 0};
}

void TrafficCounter::begin() {
    s_instance = this;

    for (uint8_t idx = 0; idx < static_cast<uint8_t>(ButtonId::Count); ++idx) {
        auto& btn = buttons_[idx];
        pinMode(btn.pin, INPUT_PULLUP);
        btn.pending = false;
        btn.lastTriggerMs = 0;
    }

    attachInterrupt(digitalPinToInterrupt(button(ButtonId::LeftEntry).pin),
                    handleLeftEntryIsr, FALLING);
    attachInterrupt(digitalPinToInterrupt(button(ButtonId::LeftExit).pin),
                    handleLeftExitIsr, FALLING);
    attachInterrupt(digitalPinToInterrupt(button(ButtonId::RightEntry).pin),
                    handleRightEntryIsr, FALLING);
    attachInterrupt(digitalPinToInterrupt(button(ButtonId::RightExit).pin),
                    handleRightExitIsr, FALLING);

    LOG_INFO(Logger::TAG_TRF,
             "TrafficCounter initialised (pins L entry=%u exit=%u | R entry=%u exit=%u)",
             button(ButtonId::LeftEntry).pin,
             button(ButtonId::LeftExit).pin,
             button(ButtonId::RightEntry).pin,
             button(ButtonId::RightExit).pin);
}

void TrafficCounter::update() {
    for (uint8_t idx = 0; idx < static_cast<uint8_t>(ButtonId::Count); ++idx) {
        auto& btn = buttons_[idx];
        bool shouldProcess = false;
#ifndef UNIT_TEST
        portENTER_CRITICAL(&s_isrMux);
#endif
        if (btn.pending) {
            btn.pending = false;
            shouldProcess = true;
        }
#ifndef UNIT_TEST
        portEXIT_CRITICAL(&s_isrMux);
#endif
        if (!shouldProcess) {
            continue;
        }
        processButton(static_cast<ButtonId>(idx));
    }
}

int TrafficCounter::getLeftCount() const {
    return leftCount_;
}

int TrafficCounter::getRightCount() const {
    return rightCount_;
}

bool TrafficCounter::isBridgeClear() const {
    return (leftCount_ + rightCount_) == 0;
}

void TrafficCounter::resetCounts() {
    int currentLeft = leftCount_;
    int currentRight = rightCount_;
    if (currentLeft == 0 && currentRight == 0) {
        return;
    }
    leftCount_ = 0;
    rightCount_ = 0;
    publishCounts(-currentLeft, -currentRight);
    LOG_INFO(Logger::TAG_TRF, "Traffic counts reset to zero");
}

#ifdef UNIT_TEST
void TrafficCounter::simulateDelta(int leftDelta, int rightDelta) {
    applyDelta(leftDelta, rightDelta);
}
#endif

void TrafficCounter::handleIsr(ButtonId id) {
    if (s_instance == nullptr) {
        return;
    }
    auto& btn = s_instance->button(id);
    const uint32_t now = millis();
    const uint32_t last = btn.lastTriggerMs;
    if (now - last < DEBOUNCE_MS) {
        return;
    }
#ifndef UNIT_TEST
    portENTER_CRITICAL_ISR(&s_isrMux);
#endif
    btn.lastTriggerMs = now;
    btn.pending = true;
#ifndef UNIT_TEST
    portEXIT_CRITICAL_ISR(&s_isrMux);
#endif
}

void TrafficCounter::processButton(ButtonId id) {
    auto& btn = button(id);
    int deltaLeft = 0;
    int deltaRight = 0;

    const int delta = (btn.role == ButtonRole::Entry) ? 1 : -1;
    if (btn.side == TrafficSide::Left) {
        deltaLeft = delta;
    } else {
        deltaRight = delta;
    }

    applyDelta(deltaLeft, deltaRight);
}

void TrafficCounter::applyDelta(int leftDelta, int rightDelta) {
    if (leftDelta == 0 && rightDelta == 0) {
        return;
    }

    const int currentLeft = leftCount_;
    const int currentRight = rightCount_;

    int newLeft = currentLeft + leftDelta;
    int newRight = currentRight + rightDelta;
    bool clamped = false;

    if (newLeft < 0) {
        LOG_WARN(Logger::TAG_TRF, "Attempted to decrement left count below zero");
        newLeft = 0;
        clamped = true;
    }
    if (newRight < 0) {
        LOG_WARN(Logger::TAG_TRF, "Attempted to decrement right count below zero");
        newRight = 0;
        clamped = true;
    }

    if (newLeft == currentLeft && newRight == currentRight) {
        return;
    }

    leftCount_ = newLeft;
    rightCount_ = newRight;

    const int appliedDeltaLeft = newLeft - currentLeft;
    const int appliedDeltaRight = newRight - currentRight;
    publishCounts(appliedDeltaLeft, appliedDeltaRight);

    LOG_INFO(Logger::TAG_TRF,
             "Traffic counts updated (L=%d [%+d], R=%d [%+d])%s",
             leftCount_,
             appliedDeltaLeft,
             rightCount_,
             appliedDeltaRight,
             clamped ? " (clamped)" : "");
}

void TrafficCounter::publishCounts(int deltaLeft, int deltaRight) {
    auto* payload = new TrafficCountEventData(
        leftCount_, rightCount_, deltaLeft, deltaRight);
    bus_.publish(BridgeEvent::TRAFFIC_COUNT_CHANGED, payload, EventPriority::NORMAL);
}

TrafficCounter::ButtonState& TrafficCounter::button(ButtonId id) {
    return buttons_[static_cast<uint8_t>(id)];
}

const TrafficCounter::ButtonState& TrafficCounter::button(ButtonId id) const {
    return buttons_[static_cast<uint8_t>(id)];
}

void IRAM_ATTR TrafficCounter::handleLeftEntryIsr() {
    if (s_instance) {
        s_instance->handleIsr(ButtonId::LeftEntry);
    }
}

void IRAM_ATTR TrafficCounter::handleLeftExitIsr() {
    if (s_instance) {
        s_instance->handleIsr(ButtonId::LeftExit);
    }
}

void IRAM_ATTR TrafficCounter::handleRightEntryIsr() {
    if (s_instance) {
        s_instance->handleIsr(ButtonId::RightEntry);
    }
}

void IRAM_ATTR TrafficCounter::handleRightExitIsr() {
    if (s_instance) {
        s_instance->handleIsr(ButtonId::RightExit);
    }
}
