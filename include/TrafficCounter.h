#pragma once

#include <Arduino.h>
#include "EventBus.h"

/**
 * TrafficCounter monitors four momentary buttons (entry/exit per side) to maintain
 * a running count of vehicles queued on each side of the bridge. Counts are used
 * for safety interlocks before opening the bridge.
 */
class TrafficCounter {
public:
    TrafficCounter(EventBus& bus);

    void begin();
    void update();

    int getLeftCount() const;
    int getRightCount() const;
    bool isBridgeClear() const;  // true when no vehicles queued on either side

    void resetCounts();

#ifdef UNIT_TEST
    void simulateDelta(int leftDelta, int rightDelta);
#endif

private:
    enum class ButtonId : uint8_t {
        LeftEntry = 0,
        LeftExit,
        RightEntry,
        RightExit,
        Count
    };

    enum class TrafficSide : uint8_t {
        Left,
        Right
    };

    enum class ButtonRole : uint8_t {
        Entry,
        Exit
    };

    struct ButtonState {
        uint8_t pin;
        TrafficSide side;
        ButtonRole role;
        volatile bool pending;
        volatile uint32_t lastTriggerMs;
    };

    static constexpr uint8_t DEFAULT_LEFT_ENTRY_PIN = 16;
    static constexpr uint8_t DEFAULT_LEFT_EXIT_PIN = 17;
    static constexpr uint8_t DEFAULT_RIGHT_ENTRY_PIN = 18;
    static constexpr uint8_t DEFAULT_RIGHT_EXIT_PIN = 19;
    static constexpr uint32_t DEBOUNCE_MS = 50;

    static TrafficCounter* s_instance;

    static void IRAM_ATTR handleLeftEntryIsr();
    static void IRAM_ATTR handleLeftExitIsr();
    static void IRAM_ATTR handleRightEntryIsr();
    static void IRAM_ATTR handleRightExitIsr();

    void handleIsr(ButtonId id);
    void processButton(ButtonId id);
    void applyDelta(int leftDelta, int rightDelta);
    void publishCounts(int deltaLeft, int deltaRight);

    ButtonState& button(ButtonId id);
    const ButtonState& button(ButtonId id) const;

    EventBus& bus_;
    volatile int leftCount_;
    volatile int rightCount_;
    ButtonState buttons_[static_cast<uint8_t>(ButtonId::Count)];
};
