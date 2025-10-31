#include "StateWriter.h"
#include "Logger.h"
#include "ConsoleCommands.h"

StateWriter::StateWriter(EventBus& bus) : bus_(bus) {}

void StateWriter::attachConsole(ConsoleCommands* console) {
    console_ = console;
}

// Below subscribes all events that the FSM / subsystems publish.
void StateWriter::beginSubscriptions() {
    using E = BridgeEvent;

    auto sub = [this](EventData* d){ this->onEvent(d); };

    bus_.subscribe(E::BOAT_DETECTED, sub);
    bus_.subscribe(E::BOAT_DETECTED_LEFT, sub);
    bus_.subscribe(E::BOAT_DETECTED_RIGHT, sub);
    bus_.subscribe(E::BOAT_PASSED, sub);
    bus_.subscribe(E::BOAT_PASSED_LEFT, sub);
    bus_.subscribe(E::BOAT_PASSED_RIGHT, sub);
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
    bus_.subscribe(E::BOAT_GREEN_PERIOD_EXPIRED, sub);
    bus_.subscribe(E::SYSTEM_RESET_REQUESTED, sub);
    bus_.subscribe(E::SIMULATION_ENABLED, sub);
    bus_.subscribe(E::SIMULATION_DISABLED, sub);
    bus_.subscribe(E::SIMULATION_SENSOR_CONFIG_CHANGED, sub);
    
    // Subscribe to actual state changes from the state machine
    bus_.subscribe(E::STATE_CHANGED, sub);
}

void StateWriter::fillBridgeStatus(JsonObject obj) const {
    std::lock_guard<std::mutex> lk(mu_);
    obj["state"] = bridgeState_;
    obj["lastChangeMs"] = bridgeLastChangeMs_;
    obj["manualMode"]   = manualMode_;
    
    // Include boat timer info if active
    if (boatTimerStartMs_ > 0) {
        obj["boatTimerStartMs"] = boatTimerStartMs_;
        obj["boatTimerSide"] = boatTimerSide_;
    } else {
        obj["boatTimerStartMs"] = 0;
        obj["boatTimerSide"] = "";
    }
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
    obj["simulation"] = simulationMode_;
    obj["logLevel"] = Logger::levelToString(Logger::getLevel());
    JsonObject sensors = obj["simulationSensors"].to<JsonObject>();
    sensors["ultrasonicLeft"] = simUltrasonicLeftEnabled_;
    sensors["ultrasonicRight"] = simUltrasonicRightEnabled_;
    sensors["beamBreak"] = simBeamBreakEnabled_;
    
    // Ultrasonic streaming state
    if (console_) {
        JsonObject streaming = obj["ultrasonicStreaming"].to<JsonObject>();
        streaming["left"] = console_->isStreamingLeft();
        streaming["right"] = console_->isStreamingRight();
    }
}

void StateWriter::buildSnapshot(JsonDocument& out) const {
    // envelope
    out["v"] = 1;
    out["type"] = "event";
    out["path"] = "/system/snapshot";
    JsonObject p = out["payload"].to<JsonObject>();

    // sections
    JsonObject bridge = p["bridge"].to<JsonObject>();
    fillBridgeStatus(bridge);

    JsonObject traffic = p["traffic"].to<JsonObject>();
    fillCarTrafficStatus(traffic["car"].to<JsonObject>());
    fillBoatTrafficStatus(traffic["boat"].to<JsonObject>());

    JsonObject system = p["system"].to<JsonObject>();
    fillSystemStatus(system);

    JsonArray logArr = p["log"].to<JsonArray>();
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
                const BridgeState previousState = stateData->getPreviousState();
                const BridgeState newState = stateData->getNewState();
                bridgeState_ = stateToString(newState);
                bridgeLastChangeMs_ = now;
                if (newState != previousState) {
                    pushLog(String("Bridge state changed: ") + stateToString(previousState) +
                            " -> " + stateToString(newState));
                }
            }
            break;
        case BridgeEvent::SIMULATION_ENABLED:
            simulationMode_ = true;
            pushLog("Simulation mode ENABLED");
            break;
        case BridgeEvent::SIMULATION_DISABLED:
            simulationMode_ = false;
            pushLog("Simulation mode DISABLED");
            break;
        case BridgeEvent::SIMULATION_SENSOR_CONFIG_CHANGED:
            if (data && data->getEventEnum() == BridgeEvent::SIMULATION_SENSOR_CONFIG_CHANGED) {
                auto* cfgData = static_cast<SimulationSensorConfigData*>(data);
                simUltrasonicLeftEnabled_ = cfgData->isUltrasonicLeftEnabled();
                simUltrasonicRightEnabled_ = cfgData->isUltrasonicRightEnabled();
                simBeamBreakEnabled_ = cfgData->isBeamBreakEnabled();
                pushLog(String("Simulation sensors updated: UL=") + (simUltrasonicLeftEnabled_ ? "ON" : "OFF") +
                        ", UR=" + (simUltrasonicRightEnabled_ ? "ON" : "OFF") +
                        ", Beam=" + (simBeamBreakEnabled_ ? "ON" : "OFF"));
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
        case BridgeEvent::BOAT_DETECTED_LEFT:
            pushLog("Event: BOAT_DETECTED_LEFT");
            break;
        case BridgeEvent::BOAT_DETECTED_RIGHT:
            pushLog("Event: BOAT_DETECTED_RIGHT");
            break;
        case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
            carLeft_ = carRight_ = "Red";
            pushLog(String("Traffic stopped; car lights now ") + carLeft_ + "/" + carRight_);
            break;
        case BridgeEvent::BRIDGE_OPENED_SUCCESS:
            bridgeLockEngaged_ = false;
            bridgeLastChangeMs_ = now;
            // Boat lights are set by startBoatGreenPeriod() which publishes BOAT_LIGHT_CHANGED_SUCCESS events
            // Don't override them here - let the individual light change events handle it
            pushLog("Bridge opened");
            break;
        case BridgeEvent::BOAT_PASSED:
            pushLog("Event: BOAT_PASSED");
            break;
        case BridgeEvent::BOAT_PASSED_LEFT:
            pushLog("Event: BOAT_PASSED_LEFT");
            break;
        case BridgeEvent::BOAT_PASSED_RIGHT:
            pushLog("Event: BOAT_PASSED_RIGHT");
            break;
        case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
            bridgeLockEngaged_ = true;
            bridgeLastChangeMs_ = now;
            boatLeft_ = boatRight_ = "Red";
            pushLog(String("Bridge closed; boat lights now ") + boatLeft_ + "/" + boatRight_);
            break;
        case BridgeEvent::TRAFFIC_RESUMED_SUCCESS:
            carLeft_ = carRight_ = "Green";
            pushLog(String("Traffic resumed; car lights now ") + carLeft_ + "/" + carRight_);
            break;
        case BridgeEvent::INDICATOR_UPDATE_SUCCESS:
            // Indicator LED updated - no need to log every state change
            // (State changes are already logged via STATE_CHANGED events)
            break;
        case BridgeEvent::FAULT_DETECTED:
            inFault_ = true;
            carLeft_ = carRight_ = "Red";
            boatLeft_ = boatRight_ = "Red";
            pushLog("EMERGENCY: FAULT_DETECTED");
            break;
        case BridgeEvent::SYSTEM_SAFE_SUCCESS:
            pushLog("System reports safe status");
            break;
        case BridgeEvent::FAULT_CLEARED:
            inFault_ = false;
            bridgeLockEngaged_ = true;
            carLeft_ = carRight_ = "Green";
            boatLeft_ = boatRight_ = "Red";
            pushLog("Event: FAULT_CLEARED");
            break;
        case BridgeEvent::SYSTEM_RESET_REQUESTED:
            inFault_ = false;
            manualMode_ = false;
            bridgeLockEngaged_ = true;
            carLeft_ = carRight_ = "Green";
            boatLeft_ = boatRight_ = "Red";
            pushLog("Command: SYSTEM_RESET_REQUESTED -> reset to idle defaults");
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
            // Handle car light changes; support combined updates via side="both"
            if (data && data->getEventEnum() == BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS) {
                auto* lightData = static_cast<LightChangeData*>(data);
                if (lightData->getSide() == "left") {
                    carLeft_ = lightData->getColor();
                } else if (lightData->getSide() == "right") {
                    carRight_ = lightData->getColor();
                } else if (lightData->getSide() == "both") {
                    carLeft_ = carRight_ = lightData->getColor();
                }
                const String& side = lightData->getSide();
                const String& color = lightData->getColor();
                if (side == "both") {
                    pushLog(String("Car lights set to ") + color);
                } else {
                    pushLog(String("Car light (") + side + ") set to " + color);
                }
            }
            break;
        case BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS:
            // Handle individual boat light changes
            if (data && data->getEventEnum() == BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS) {
                auto* lightData = static_cast<LightChangeData*>(data);
                const String& side = lightData->getSide();
                const String& color = lightData->getColor();
                
                if (side == "left") {
                    boatLeft_ = color;
                } else if (side == "right") {
                    boatRight_ = color;
                }
                
                // Track boat timer: if a side turns green, start timer. If both turn red, stop timer.
                if (color == "Green") {
                    boatTimerStartMs_ = now;
                    boatTimerSide_ = side;
                } else if (color == "Red" && (boatLeft_ == "Red" && boatRight_ == "Red")) {
                    // Both lights red = timer inactive
                    boatTimerStartMs_ = 0;
                    boatTimerSide_ = "";
                }
                
                pushLog(String("Boat light (") + side + ") set to " + color);
            }
            break;
        case BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED:
            // Timer expired - reset timer state
            boatTimerStartMs_ = 0;
            boatTimerSide_ = "";
            break;
        default:
            pushLog(String("Event: ") + eventName(ev));
            break;
    }
}

void StateWriter::pushLog(const String& line) {
    if (log_.size() >= LOG_CAP_) log_.erase(log_.begin());
    // Assign a monotonically increasing sequence so clients can deduplicate without dropping repeats
    log_.push_back(String(logSeq_++) + "|" + line);
}

const char* StateWriter::eventName(BridgeEvent ev) {
  switch (ev) {
    case BridgeEvent::BOAT_DETECTED: return "BOAT_DETECTED";
    case BridgeEvent::BOAT_DETECTED_LEFT: return "BOAT_DETECTED_LEFT";
    case BridgeEvent::BOAT_DETECTED_RIGHT: return "BOAT_DETECTED_RIGHT";
    case BridgeEvent::BOAT_PASSED: return "BOAT_PASSED";
    case BridgeEvent::BOAT_PASSED_LEFT: return "BOAT_PASSED_LEFT";
    case BridgeEvent::BOAT_PASSED_RIGHT: return "BOAT_PASSED_RIGHT";
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
    case BridgeEvent::SIMULATION_SENSOR_CONFIG_CHANGED: return "SIMULATION_SENSOR_CONFIG_CHANGED";
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
