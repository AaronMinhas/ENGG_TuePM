#include "StateWriter.h"

StateWriter::StateWriter(EventBus& bus) : bus_(bus) {}

// Below subscribes all events that the FSM / subsystems publish.
void StateWriter::beginSubscriptions() {
    using E = BridgeEvent;

    auto sub = [this](EventData* d){ this->onEvent(d); };

    bus_.subscribe(E::BOAT_DETECTED, sub);
    bus_.subscribe(E::BOAT_PASSED, sub);
    bus_.subscribe(E::FAULT_DETECTED, sub);
    bus_.subscribe(E::FAULT_CLEARED, sub);
    bus_.subscribe(E::MANUAL_OVERRIDE_ACTIVATED, sub);
    bus_.subscribe(E::MANUAL_OVERRIDE_DEACTIVATED, sub);

    bus_.subscribe(E::TRAFFIC_STOPPED_SUCCESS, sub);
    bus_.subscribe(E::BRIDGE_OPENED_SUCCESS, sub);
    bus_.subscribe(E::BRIDGE_CLOSED_SUCCESS, sub);
    bus_.subscribe(E::TRAFFIC_RESUMED_SUCCESS, sub);
    bus_.subscribe(E::INDICATOR_UPDATE_SUCCESS, sub);
    bus_.subscribe(E::SYSTEM_SAFE_SUCCESS, sub);
    bus_.subscribe(E::CAR_LIGHT_CHANGED_SUCCESS, sub);
    bus_.subscribe(E::BOAT_LIGHT_CHANGED_SUCCESS, sub);
    
    // Subscribe to actual state changes from the state machine
    bus_.subscribe(E::STATE_CHANGED, sub);
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

void StateWriter::applyEvent(BridgeEvent ev, EventData* data) {
    const uint32_t now = millis();
    std::lock_guard<std::mutex> lk(mu_);

    switch (ev) {
        case BridgeEvent::STATE_CHANGED:
            // Handle actual state changes from the state machine
            if (data && data->getEventEnum() == BridgeEvent::STATE_CHANGED) {
                // Safe cast
                auto* stateData = static_cast<StateChangeData*>(data);
                bridgeState_ = stateToString(stateData->getNewState());
                bridgeLastChangeMs_ = now;
                pushLog(String("State: ") + stateToString(stateData->getPreviousState()) + 
                       " -> " + stateToString(stateData->getNewState()));
            }
            break;
            
        case BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED:
            // Log the request but don't change state - wait for STATE_CHANGED
            pushLog("Request: MANUAL_BRIDGE_OPEN_REQUESTED");
            break;
        case BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED:
            // Log the request but don't change state - wait for STATE_CHANGED
            pushLog("Request: MANUAL_BRIDGE_CLOSE_REQUESTED");
            break;
        case BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED:
            carLeft_ = carRight_ = "Red";
            pushLog("Request: MANUAL_TRAFFIC_STOP_REQUESTED (car=Red,Red)");
            break;
        case BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED:
            carLeft_ = carRight_ = "Green";
            pushLog("Request: MANUAL_TRAFFIC_RESUME_REQUESTED (car=Green,Green)");
            break;
        case BridgeEvent::BOAT_DETECTED:
            pushLog("Event: BOAT_DETECTED");
            break;
        case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
            carLeft_ = carRight_ = "Red";
            pushLog("Success: TRAFFIC_STOPPED (car=Red,Red)");
            break;
        case BridgeEvent::BRIDGE_OPENED_SUCCESS:
            bridgeLockEngaged_ = false;
            bridgeLastChangeMs_ = now;
            boatLeft_ = boatRight_ = "Green";
            pushLog("Success: BRIDGE_OPENED (boat=Green,Green)");
            break;
        case BridgeEvent::BOAT_PASSED:
            pushLog("Event: BOAT_PASSED");
            break;
        case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
            bridgeLockEngaged_ = true;
            bridgeLastChangeMs_ = now;
            boatLeft_ = boatRight_ = "Red";
            pushLog("Success: BRIDGE_CLOSED (boat=Red,Red)");
            break;
        case BridgeEvent::TRAFFIC_RESUMED_SUCCESS:
            carLeft_ = carRight_ = "Green";
            pushLog("Success: TRAFFIC_RESUMED (car=Green,Green)");
            break;
        case BridgeEvent::INDICATOR_UPDATE_SUCCESS:
            pushLog("Success: INDICATOR_UPDATE");
            break;
        case BridgeEvent::FAULT_DETECTED:
            inFault_ = true;
            carLeft_ = carRight_ = "Red";
            boatLeft_ = boatRight_ = "Red";
            pushLog("EMERGENCY: FAULT_DETECTED");
            break;
        case BridgeEvent::SYSTEM_SAFE_SUCCESS:
            pushLog("Success: SYSTEM_SAFE");
            break;
        case BridgeEvent::FAULT_CLEARED:
            inFault_ = false;
            bridgeLockEngaged_ = true;
            carLeft_ = carRight_ = "Green";
            boatLeft_ = boatRight_ = "Red";
            pushLog("Event: FAULT_CLEARED");
            break;
        case BridgeEvent::MANUAL_OVERRIDE_ACTIVATED:
            manualMode_ = true;
            pushLog("Event: MANUAL_OVERRIDE_ACTIVATED");
            break;
        case BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED:
            manualMode_ = false;
            pushLog("Event: MANUAL_OVERRIDE_DEACTIVATED");
            break;
        case BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS:
            // Handle individual car light changes
            if (data && data->getEventEnum() == BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS) {
                auto* lightData = static_cast<LightChangeData*>(data);
                if (lightData->getSide() == "left") {
                    carLeft_ = lightData->getColor();
                } else if (lightData->getSide() == "right") {
                    carRight_ = lightData->getColor();
                }
                pushLog("Success: CAR_LIGHT_CHANGED (" + lightData->getSide() + "=" + lightData->getColor() + ")");
            }
            break;
        case BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS:
            // Handle individual boat light changes
            if (data && data->getEventEnum() == BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS) {
                auto* lightData = static_cast<LightChangeData*>(data);
                if (lightData->getSide() == "left") {
                    boatLeft_ = lightData->getColor();
                } else if (lightData->getSide() == "right") {
                    boatRight_ = lightData->getColor();
                }
                pushLog("Success: BOAT_LIGHT_CHANGED (" + lightData->getSide() + "=" + lightData->getColor() + ")");
            }
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
    case BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED: return "MANUAL_BRIDGE_OPEN_REQUESTED";
    case BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED: return "MANUAL_BRIDGE_CLOSE_REQUESTED";
    case BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED: return "MANUAL_TRAFFIC_STOP_REQUESTED";
    case BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED: return "MANUAL_TRAFFIC_RESUME_REQUESTED";
    case BridgeEvent::STATE_CHANGED: return "STATE_CHANGED";
    default: return "UNKNOWN_EVENT";
  }
}

const char* StateWriter::stateToString(BridgeState state) {
  switch (state) {
    case BridgeState::IDLE: return "IDLE";
    case BridgeState::STOPPING_TRAFFIC: return "STOPPING_TRAFFIC";
    case BridgeState::OPENING: return "OPENING";
    case BridgeState::OPEN: return "OPEN";
    case BridgeState::CLOSING: return "CLOSING";
    case BridgeState::RESUMING_TRAFFIC: return "RESUMING_TRAFFIC";
    case BridgeState::FAULT: return "FAULT";
    case BridgeState::MANUAL_MODE: return "MANUAL_MODE";
    case BridgeState::MANUAL_OPENING: return "MANUAL_OPENING";
    case BridgeState::MANUAL_OPEN: return "MANUAL_OPEN";
    case BridgeState::MANUAL_CLOSING: return "MANUAL_CLOSING";
    case BridgeState::MANUAL_CLOSED: return "MANUAL_CLOSED";
    default: return "UNKNOWN_STATE";
  }
}
