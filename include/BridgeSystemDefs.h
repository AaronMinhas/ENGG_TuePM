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
