#include "StateWriter.h"

StateWriter::StateWriter() {}

// Below subscribes all events that the FSM / subsystems publish.
void StateWriter::beginSubscriptions() {
    using E = BridgeEvent;

    auto sub = [this](EventData* d){ this->onEvent(d); };

    eventBus.subscribe(E::BOAT_DETECTED, sub);
    eventBus.subscribe(E::BOAT_PASSED, sub);
    eventBus.subscribe(E::FAULT_DETECTED, sub);
    eventBus.subscribe(E::FAULT_CLEARED, sub);
    eventBus.subscribe(E::MANUAL_OVERRIDE_ACTIVATED, sub);
    eventBus.subscribe(E::MANUAL_OVERRIDE_DEACTIVATED, sub);

    eventBus.subscribe(E::TRAFFIC_STOPPED_SUCCESS, sub);
    eventBus.subscribe(E::BRIDGE_OPENED_SUCCESS, sub);
    eventBus.subscribe(E::BRIDGE_CLOSED_SUCCESS, sub);
    eventBus.subscribe(E::TRAFFIC_RESUMED_SUCCESS, sub);
    eventBus.subscribe(E::INDICATOR_UPDATE_SUCCESS, sub);
    eventBus.subscribe(E::SYSTEM_SAFE_SUCCESS, sub);
}

void StateWriter::fillBridgeStatus(JsonObject obj) const {
    std::lock_guard<std::mutex> lk(mu_);
    obj["state"] = bridgeState_;
    obj["lastChangeMs"] = bridgeLastChangeMs_;
    obj["manualMode"]   = manualMode_;
}

void StateWriter::fillCarTrafficStatus(JsonObject obj) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto left = obj.createNestedObject("left");
    left["value"] = carLeft_;
    auto right = obj.createNestedObject("right");
    right["value"] = carRight_;
}

void StateWriter::fillBoatTrafficStatus(JsonObject obj) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto left = obj.createNestedObject("left");
    left["value"] = boatLeft_;
    auto right = obj.createNestedObject("right");
    right["value"] = boatRight_;
}

void StateWriter::fillSystemStatus(JsonObject obj) const {
    std::lock_guard<std::mutex> lk(mu_);
    obj["connection"] = "Connected";
}

void StateWriter::buildSnapshot(JsonDocument& out) const {
    // envelope
    out["v"] = 1;
    out["type"] = "event";
    out["path"] = "/system/snapshot";
    JsonObject p = out.createNestedObject("payload");

    // sections
    JsonObject bridge = p.createNestedObject("bridge");
    fillBridgeStatus(bridge);

    JsonObject traffic = p.createNestedObject("traffic");
    fillCarTrafficStatus(traffic.createNestedObject("car"));
    fillBoatTrafficStatus(traffic.createNestedObject("boat"));

    JsonObject system = p.createNestedObject("system");
    fillSystemStatus(system);

    JsonArray logArr = p.createNestedArray("log");
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& s : log_) logArr.add(s);
    }
}

std::vector<String> StateWriter::getActivityLog() const {
    std::lock_guard<std::mutex> lk(mu_);
    return log_;
}

void StateWriter::onEvent(EventData* data) {
    if (!data) return;
    applyEvent(data->getEventEnum(), data);
}

void StateWriter::applyEvent(BridgeEvent ev, EventData*) {
    const uint32_t now = millis();
    std::lock_guard<std::mutex> lk(mu_);

    switch (ev) {
        case BridgeEvent::BOAT_DETECTED:
            bridgeState_ = "WAITING_FOR_BOAT";
            pushLog("Event: BOAT_DETECTED");
            break;
        case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
            bridgeState_ = "OPENING";
            carLeft_ = carRight_ = "Red";
            pushLog("Success: TRAFFIC_STOPPED (car=Red,Red)");
            break;
        case BridgeEvent::BRIDGE_OPENED_SUCCESS:
            bridgeState_ = "OPEN";
            bridgeLockEngaged_ = false;
            bridgeLastChangeMs_ = now;
            boatLeft_ = boatRight_ = "Green";
            pushLog("Success: BRIDGE_OPENED (boat=Green,Green)");
            break;
        case BridgeEvent::BOAT_PASSED:
            bridgeState_ = "CLOSING";
            pushLog("Event: BOAT_PASSED");
            break;
        case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
            bridgeState_ = "RESUMING_TRAFFIC";
            bridgeLockEngaged_ = true;
            bridgeLastChangeMs_ = now;
            boatLeft_ = boatRight_ = "Red";
            pushLog("Success: BRIDGE_CLOSED (boat=Red,Red)");
            break;
        case BridgeEvent::TRAFFIC_RESUMED_SUCCESS:
            bridgeState_ = "IDLE";
            carLeft_ = carRight_ = "Green";
            pushLog("Success: TRAFFIC_RESUMED (car=Green,Green)");
            break;
        case BridgeEvent::INDICATOR_UPDATE_SUCCESS:
            pushLog("Success: INDICATOR_UPDATE");
            break;
        case BridgeEvent::FAULT_DETECTED:
            inFault_ = true;
            bridgeState_ = "FAULT";
            carLeft_ = carRight_ = "Red";
            boatLeft_ = boatRight_ = "Red";
            pushLog("EMERGENCY: FAULT_DETECTED");
            break;
        case BridgeEvent::SYSTEM_SAFE_SUCCESS:
            pushLog("Success: SYSTEM_SAFE");
            break;
        case BridgeEvent::FAULT_CLEARED:
            inFault_ = false;
            bridgeState_ = "IDLE";
            bridgeLockEngaged_ = true;
            carLeft_ = carRight_ = "Green";
            boatLeft_ = boatRight_ = "Red";
            pushLog("Event: FAULT_CLEARED -> IDLE");
            break;
        case BridgeEvent::MANUAL_OVERRIDE_ACTIVATED:
            manualMode_ = true;
            bridgeState_ = "MANUAL_MODE";
            pushLog("Event: MANUAL_OVERRIDE_ACTIVATED");
            break;
        case BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED:
            manualMode_ = false;
            bridgeState_ = "IDLE";
            pushLog("Event: MANUAL_OVERRIDE_DEACTIVATED -> IDLE");
            break;
        default:
            pushLog(String("Event: ") + eventName(ev));
            break;
    }
}

void StateWriter::pushLog(const String& line) {
    if (log_.size() >= LOG_CAP_) log_.erase(log_.begin());
    char ts[32];
    snprintf(ts, sizeof(ts), "%lu", (unsigned long)millis());
    log_.push_back(String("[") + ts + "] " + line);
}

const char* StateWriter::eventName(BridgeEvent ev) {
  switch (ev) {
    case BridgeEvent::BOAT_DETECTED: return "BOAT_DETECTED";
    case BridgeEvent::BOAT_PASSED: return "BOAT_PASSED";
    case BridgeEvent::TRAFFIC_STOPPED_SUCCESS: return "TRAFFIC_STOPPED_SUCCESS";
    case BridgeEvent::BRIDGE_OPENED_SUCCESS: return "BRIDGE_OPENED_SUCCESS";
    case BridgeEvent::BRIDGE_CLOSED_SUCCESS: return "BRIDGE_CLOSED_SUCCESS";
    case BridgeEvent::TRAFFIC_RESUMED_SUCCESS: return "TRAFFIC_RESUMED_SUCCESS";
    case BridgeEvent::INDICATOR_UPDATE_SUCCESS: return "INDICATOR_UPDATE_SUCCESS";
    case BridgeEvent::SYSTEM_SAFE_SUCCESS: return "SYSTEM_SAFE_SUCCESS";
    case BridgeEvent::FAULT_DETECTED: return "FAULT_DETECTED";
    case BridgeEvent::FAULT_CLEARED: return "FAULT_CLEARED";
    case BridgeEvent::MANUAL_OVERRIDE_ACTIVATED: return "MANUAL_OVERRIDE_ACTIVATED";
    case BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED: return "MANUAL_OVERRIDE_DEACTIVATED";
    default: return "UNKNOWN_EVENT";
  }
}
