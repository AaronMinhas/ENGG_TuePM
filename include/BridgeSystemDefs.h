#pragma once
#include <Arduino.h>

// Boat Traffic Queue Timing Constants
#define BOAT_GREEN_PERIOD_MS 45000      // 45 seconds - boats can pass during this time
#define BOAT_PASSAGE_TIMEOUT_MS 120000  // 2 minutes - emergency timeout if no boat passes
#define BOAT_CYCLE_COOLDOWN_MS 45000    // 45 seconds - buffer before starting a new bridge cycle

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
    BEAM_BREAK_ACTIVE,
    BEAM_BREAK_CLEAR,
    FAULT_DETECTED,
    FAULT_CLEARED,
    MANUAL_OVERRIDE_ACTIVATED,
    MANUAL_OVERRIDE_DEACTIVATED,
    SYSTEM_RESET_REQUESTED,

    // Simulation Mode Events
    SIMULATION_ENABLED,
    SIMULATION_DISABLED,
    SIMULATION_SENSOR_CONFIG_CHANGED,

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
    
    // Boat Queue Events
    BOAT_GREEN_PERIOD_EXPIRED,    // 45-second green period has ended
    BOAT_PASSAGE_TIMEOUT,          // Emergency timeout - boat didn't pass
    
    // State Change Events
    STATE_CHANGED
};

// Convert BridgeEvent to readable string
inline const char* bridgeEventToString(BridgeEvent event) {
    switch (event) {
        case BridgeEvent::BOAT_DETECTED: return "BOAT_DETECTED";
        case BridgeEvent::BOAT_PASSED: return "BOAT_PASSED";
        case BridgeEvent::BOAT_DETECTED_LEFT: return "BOAT_DETECTED_LEFT";
        case BridgeEvent::BOAT_DETECTED_RIGHT: return "BOAT_DETECTED_RIGHT";
        case BridgeEvent::BOAT_PASSED_LEFT: return "BOAT_PASSED_LEFT";
        case BridgeEvent::BOAT_PASSED_RIGHT: return "BOAT_PASSED_RIGHT";
        case BridgeEvent::BEAM_BREAK_ACTIVE: return "BEAM_BREAK_ACTIVE";
        case BridgeEvent::BEAM_BREAK_CLEAR: return "BEAM_BREAK_CLEAR";
        case BridgeEvent::FAULT_DETECTED: return "FAULT_DETECTED";
        case BridgeEvent::FAULT_CLEARED: return "FAULT_CLEARED";
        case BridgeEvent::MANUAL_OVERRIDE_ACTIVATED: return "MANUAL_OVERRIDE_ACTIVATED";
        case BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED: return "MANUAL_OVERRIDE_DEACTIVATED";
        case BridgeEvent::SYSTEM_RESET_REQUESTED: return "SYSTEM_RESET_REQUESTED";
        case BridgeEvent::SIMULATION_ENABLED: return "SIMULATION_ENABLED";
        case BridgeEvent::SIMULATION_DISABLED: return "SIMULATION_DISABLED";
        case BridgeEvent::SIMULATION_SENSOR_CONFIG_CHANGED: return "SIMULATION_SENSOR_CONFIG_CHANGED";
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
        case BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED: return "BOAT_GREEN_PERIOD_EXPIRED";
        case BridgeEvent::BOAT_PASSAGE_TIMEOUT: return "BOAT_PASSAGE_TIMEOUT";
        case BridgeEvent::STATE_CHANGED: return "STATE_CHANGED";
        default: return "UNKNOWN_EVENT";
    }
}

// System Command Targets
enum class CommandTarget {
    CONTROLLER,
    MOTOR_CONTROL,
    SIGNAL_CONTROL,
    LOCAL_STATE_INDICATOR,
    SAFETY_MANAGER
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
    START_BOAT_GREEN_PERIOD,
    END_BOAT_GREEN_PERIOD,

    // LocalStateIndicator Actions
    SET_STATE,

    // Controller actions
    RESET_TO_IDLE_STATE
};

// Command structure that is sent over the CommandBus.
struct Command {
    CommandTarget target;
    CommandAction action;
    String data;
};
