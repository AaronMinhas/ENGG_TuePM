#pragma once
#include <Arduino.h>

// System States
enum class BridgeState {
    IDLE,
    STOPPING_TRAFFIC,
    OPENING,
    OPEN,
    CLOSING,
    RESUMING_TRAFFIC,
    FAULT,
    MANUAL_MODE,
    
    // Manual Control States (Command Mode)
    MANUAL_OPENING,
    MANUAL_OPEN,
    MANUAL_CLOSING,
    MANUAL_CLOSED
};

// System Events
enum class BridgeEvent {
    // External Events
    BOAT_DETECTED,
    BOAT_PASSED,
    BOAT_DETECTED_LEFT,
    BOAT_DETECTED_RIGHT,
    BOAT_PASSED_LEFT,
    BOAT_PASSED_RIGHT,
    FAULT_DETECTED,
    FAULT_CLEARED,
    MANUAL_OVERRIDE_ACTIVATED,
    MANUAL_OVERRIDE_DEACTIVATED,

    // Manual Control Events (Command Mode)
    MANUAL_BRIDGE_OPEN_REQUESTED,
    MANUAL_BRIDGE_CLOSE_REQUESTED,
    MANUAL_TRAFFIC_STOP_REQUESTED,
    MANUAL_TRAFFIC_RESUME_REQUESTED,

    // Success Events
    TRAFFIC_STOPPED_SUCCESS,
    BRIDGE_OPENED_SUCCESS,
    BRIDGE_CLOSED_SUCCESS,
    TRAFFIC_RESUMED_SUCCESS,
    SYSTEM_SAFE_SUCCESS,
    INDICATOR_UPDATE_SUCCESS,
    CAR_LIGHT_CHANGED_SUCCESS,
    BOAT_LIGHT_CHANGED_SUCCESS,
    
    // State Change Events
    STATE_CHANGED
};

// Convert BridgeEvent to readable string
inline const char* bridgeEventToString(BridgeEvent event) {
    switch (event) {
        case BridgeEvent::BOAT_DETECTED: return "BOAT_DETECTED";
        case BridgeEvent::BOAT_PASSED: return "BOAT_PASSED";
        case BridgeEvent::FAULT_DETECTED: return "FAULT_DETECTED";
        case BridgeEvent::FAULT_CLEARED: return "FAULT_CLEARED";
        case BridgeEvent::MANUAL_OVERRIDE_ACTIVATED: return "MANUAL_OVERRIDE_ACTIVATED";
        case BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED: return "MANUAL_OVERRIDE_DEACTIVATED";
        case BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED: return "MANUAL_BRIDGE_OPEN_REQUESTED";
        case BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED: return "MANUAL_BRIDGE_CLOSE_REQUESTED";
        case BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED: return "MANUAL_TRAFFIC_STOP_REQUESTED";
        case BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED: return "MANUAL_TRAFFIC_RESUME_REQUESTED";
        case BridgeEvent::TRAFFIC_STOPPED_SUCCESS: return "TRAFFIC_STOPPED_SUCCESS";
        case BridgeEvent::BRIDGE_OPENED_SUCCESS: return "BRIDGE_OPENED_SUCCESS";
        case BridgeEvent::BRIDGE_CLOSED_SUCCESS: return "BRIDGE_CLOSED_SUCCESS";
        case BridgeEvent::TRAFFIC_RESUMED_SUCCESS: return "TRAFFIC_RESUMED_SUCCESS";
        case BridgeEvent::SYSTEM_SAFE_SUCCESS: return "SYSTEM_SAFE_SUCCESS";
        case BridgeEvent::INDICATOR_UPDATE_SUCCESS: return "INDICATOR_UPDATE_SUCCESS";
        case BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS: return "CAR_LIGHT_CHANGED_SUCCESS";
        case BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS: return "BOAT_LIGHT_CHANGED_SUCCESS";
        case BridgeEvent::STATE_CHANGED: return "STATE_CHANGED";
        default: return "UNKNOWN_EVENT";
    }
}

// System Command Targets
enum class CommandTarget {
    CONTROLLER,
    MOTOR_CONTROL,
    SIGNAL_CONTROL,
    LOCAL_STATE_INDICATOR
};

enum class CommandAction {
    // Controller Actions
    ENTER_SAFE_STATE,

    // MotorControl Actions
    RAISE_BRIDGE,
    LOWER_BRIDGE,

    // SignalControl Actions
    STOP_TRAFFIC,
    RESUME_TRAFFIC,
    SET_CAR_TRAFFIC,  // Set both car lights to same value
    SET_BOAT_LIGHT_LEFT,
    SET_BOAT_LIGHT_RIGHT,

    // LocalStateIndicator Actions
    SET_STATE
};

// Command structure that is sent over the CommandBus.
struct Command {
    CommandTarget target;
    CommandAction action;
    String data;
};
