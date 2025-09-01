#include "BridgeStateMachine.h"
#include <Arduino.h>

/*
 * TLDR:
 * there is a lot more to do.... basically nothing is implemented apart from foundational logic but here's a list of things i can think of which have been done and need to be done:
 * 
 * implemented :)
 *  [x] Core state machine logic with proper event-driven transitions
 *  [x] EventBus integration for receiving success events from subsystems
 *  [x] CommandBus integration for issuing commands to subsystems
 *  [x] Basic fault detection and manual override handling
 *  * COMMUNICATION PROTOCOLS
 *  [x] WebSocket server integration not connected to state machine
 *  [x] StateWriter doesn't even exist.
 *  [x] No status broadcasting to monitoring systems
 * 
 * Missing :(
 * SAFETY MANAGER INTEGRATION
 *  [] No SafetyManager subsystem implemented yet
 *  [] ENTER_SAFE_STATE command has no target to handle it
 *  [] Safety timeouts and watchdog functionality missing
 * 
 * MULTIPLE BOAT DETECTION HANDLING
 *  [] Currently ignores BOAT_DETECTED if already in progress
 *  [] No queuing system for multiple boats
 *  [] No priority handling for emergency vessels (definitely out of scope but would be cool)
 *  [] Can cause system to miss boats or create unsafe conditions
 * 
 * ERROR HANDLING & RECOVERY
 *  [] No timeout handling for waiting states (what if a success event never comes?)
 *  [] No retry logic for failed commands
 *  [] No graceful degradation or failsafe mechanisms
 *  [] Missing error event types (COMMAND_FAILED, TIMEOUT_OCCURRED, etc.)
 * 
 * STATE PERSISTENCE & RECOVERY
 *  [] No state persistence across system resets (do we even care?)
 *  [] No recovery mechanism if system crashes mid-operation
 *  [] No state validation on startup (we definitely care about this)
 * 
 * SUBSYSTEM LOGIC (doesn't affect the statemachine logic but i'll list it here anyway)
 *  [] SignalControl subsystem not implemented (no TRAFFIC_STOPPED/RESUMED_SUCCESS events)
 *  [] DetectionSystem subsystem 
 *  [] Missing subsystem command handlers in Controller
 * 
 */

BridgeStateMachine::BridgeStateMachine(EventBus& eventBus, CommandBus& commandBus)
    : m_eventBus(eventBus),
      m_commandBus(commandBus),
      m_currentState(BridgeState::IDLE),
      m_previousState(BridgeState::IDLE),
      m_stateEntryTime(0) {
}

void BridgeStateMachine::begin() {
    changeState(BridgeState::IDLE);
    subscribeToEvents();
    Serial.println("STATE MACHINE: Initialised and subscribed to EventBus");
}

void BridgeStateMachine::handleEvent(const BridgeEvent& event) {
    // Only log important state transitions, not every event
    
    // Global event handling. Takes precedence over state specific events
    // Does not implement Safety Manager but that is low priority atp.
    if (event == BridgeEvent::FAULT_DETECTED) {
        if (m_currentState != BridgeState::FAULT) {
            Serial.println("STATE: FAULT detected → entering FAULT state");
            changeState(BridgeState::FAULT);
            issueCommand(CommandTarget::CONTROLLER, CommandAction::ENTER_SAFE_STATE);
        }
        return;
    }

    if (event == BridgeEvent::MANUAL_OVERRIDE_ACTIVATED) {
        if (m_currentState != BridgeState::MANUAL_MODE) {
            Serial.println("STATE MACHINE: MANUAL_OVERRIDE_ACTIVATED - entering MANUAL_MODE");
            changeState(BridgeState::MANUAL_MODE);
            // No command issued - manual mode pauses the state machine
        }
        return;
    }

    // State specific event handling - states only change on SUCCESS events
    switch (m_currentState) {
        case BridgeState::IDLE:
            // IDLE state handles trigger events and success confirmations
            if (event == BridgeEvent::BOAT_DETECTED) {
                Serial.println("STATE MACHINE: BOAT_DETECTED in IDLE - issuing STOP_TRAFFIC command");
                // Issue command but STAY in IDLE until success confirmation
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
                Serial.println("STATE MACHINE: Staying in IDLE, waiting for TRAFFIC_STOPPED_SUCCESS...");
            } else if (event == BridgeEvent::TRAFFIC_STOPPED_SUCCESS) {
                Serial.println("STATE MACHINE: TRAFFIC_STOPPED_SUCCESS received - transitioning to STOPPING_TRAFFIC");
                changeState(BridgeState::STOPPING_TRAFFIC);
                // Now issue the next command: raise bridge
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
                Serial.println("STATE MACHINE: Now waiting for BRIDGE_OPENED_SUCCESS...");
            } else if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                Serial.println("STATE: Bridge open requested → opening");
                changeState(BridgeState::MANUAL_OPENING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
            } else if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                Serial.println("STATE: Bridge close requested → closing");
                changeState(BridgeState::MANUAL_CLOSING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::LOWER_BRIDGE);
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_TRAFFIC_STOP_REQUESTED in IDLE - issuing STOP_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
                Serial.println("STATE MACHINE: Traffic stop requested manually, staying in IDLE...");
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_TRAFFIC_RESUME_REQUESTED in IDLE - issuing RESUME_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
                Serial.println("STATE MACHINE: Traffic resume requested manually, staying in IDLE...");
            }
            break;

        case BridgeState::STOPPING_TRAFFIC:
            // STOPPING_TRAFFIC state waits ONLY for bridge to be confirmed fully open
            if (event == BridgeEvent::BRIDGE_OPENED_SUCCESS) {
                Serial.println("STATE MACHINE: BRIDGE_OPENED_SUCCESS received - transitioning to OPENING");
                changeState(BridgeState::OPENING);
                // No command issued - just waiting for boat to pass
                Serial.println("STATE MACHINE: Now waiting for BOAT_PASSED...");
            } else {
                Serial.println("STATE MACHINE: STOPPING_TRAFFIC state ignoring non-success event - still waiting for BRIDGE_OPENED_SUCCESS");
            }
            break;

        case BridgeState::OPENING:
            // OPENING state waits ONLY for boat to pass
            if (event == BridgeEvent::BOAT_PASSED) {
                Serial.println("STATE MACHINE: BOAT_PASSED received - transitioning to OPEN");
                changeState(BridgeState::OPEN);
                // Issue command to lower bridge
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::LOWER_BRIDGE);
                Serial.println("STATE MACHINE: Now waiting for BRIDGE_CLOSED_SUCCESS...");
            } else {
                Serial.println("STATE MACHINE: OPENING state ignoring non-relevant event - still waiting for BOAT_PASSED");
            }
            break;

        case BridgeState::OPEN:
            // OPEN state waits ONLY for bridge to be confirmed fully closed
            if (event == BridgeEvent::BRIDGE_CLOSED_SUCCESS) {
                Serial.println("STATE MACHINE: BRIDGE_CLOSED_SUCCESS received - transitioning to CLOSING");
                changeState(BridgeState::CLOSING);
                // Issue command to resume traffic
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
                Serial.println("STATE MACHINE: Now waiting for TRAFFIC_RESUMED_SUCCESS...");
            } else {
                Serial.println("STATE MACHINE: OPEN state ignoring non-success event - still waiting for BRIDGE_CLOSED_SUCCESS");
            }
            break;

        case BridgeState::CLOSING:
            // CLOSING state waits ONLY for traffic to be confirmed resumed
            if (event == BridgeEvent::TRAFFIC_RESUMED_SUCCESS) {
                Serial.println("STATE MACHINE: TRAFFIC_RESUMED_SUCCESS received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // No entry action - back to idle, ready for next boat
                Serial.println("STATE MACHINE: Bridge operation complete - ready for next boat");
            } else {
                Serial.println("STATE MACHINE: CLOSING state ignoring non-success event - still waiting for TRAFFIC_RESUMED_SUCCESS");
            }
            break;

        case BridgeState::FAULT:
            // FAULT state waits ONLY for fault to be cleared
            if (event == BridgeEvent::FAULT_CLEARED) {
                Serial.println("STATE MACHINE: FAULT_CLEARED received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // System should be reset to safe state (bridge closed, traffic flowing)
                Serial.println("STATE MACHINE: Fault cleared - system should be in safe state");
            } else {
                Serial.println("STATE MACHINE: FAULT state ignoring non-clear event - still waiting for FAULT_CLEARED");
            }
            break;

        case BridgeState::MANUAL_MODE:
            // MANUAL_MODE waits ONLY for manual override to be deactivated
            if (event == BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED) {
                Serial.println("STATE MACHINE: MANUAL_OVERRIDE_DEACTIVATED received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // Assume operator has returned system to safe state
                Serial.println("STATE MACHINE: Manual mode deactivated - assuming system in safe state");
            } else {
                Serial.println("STATE MACHINE: MANUAL_MODE ignoring operational events - still in manual mode");
            }
            // In manual mode, the state machine doesn't process normal operational events
            break;

        case BridgeState::MANUAL_OPENING:
            // MANUAL_OPENING state waits for bridge to finish opening
            if (event == BridgeEvent::BRIDGE_OPENED_SUCCESS) {
                Serial.println("STATE MACHINE: BRIDGE_OPENED_SUCCESS received - transitioning to MANUAL_OPEN");
                changeState(BridgeState::MANUAL_OPEN);
                Serial.println("STATE MACHINE: Bridge manually opened - waiting for manual close command");
            } else if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_BRIDGE_CLOSE_REQUESTED while opening - will close after opening completes");
            } else {
                Serial.println("STATE MACHINE: MANUAL_OPENING state ignoring non-success event - still waiting for BRIDGE_OPENED_SUCCESS");
            }
            break;

        case BridgeState::MANUAL_OPEN:
            // MANUAL_OPEN state waits for manual close command
            if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_BRIDGE_CLOSE_REQUESTED received - issuing LOWER_BRIDGE command");
                changeState(BridgeState::MANUAL_CLOSING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::LOWER_BRIDGE);
                Serial.println("STATE MACHINE: Now in MANUAL_CLOSING, waiting for BRIDGE_CLOSED_SUCCESS...");
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_TRAFFIC_STOP_REQUESTED while bridge open - issuing STOP_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_TRAFFIC_RESUME_REQUESTED while bridge open - issuing RESUME_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
            } else {
                Serial.println("STATE MACHINE: MANUAL_OPEN state waiting for manual close command");
            }
            break;

        case BridgeState::MANUAL_CLOSING:
            // MANUAL_CLOSING state waits for bridge to finish closing
            if (event == BridgeEvent::BRIDGE_CLOSED_SUCCESS) {
                Serial.println("STATE MACHINE: BRIDGE_CLOSED_SUCCESS received - returning to IDLE");
                changeState(BridgeState::IDLE);
                Serial.println("STATE MACHINE: Bridge manually closed - back to IDLE state");
            } else if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_BRIDGE_OPEN_REQUESTED while closing - will open after closing completes");
            } else {
                Serial.println("STATE MACHINE: MANUAL_CLOSING state ignoring non-success event - still waiting for BRIDGE_CLOSED_SUCCESS");
            }
            break;

        case BridgeState::MANUAL_CLOSED:
            // MANUAL_CLOSED state waits for manual open command (unused at the momement)
            if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                Serial.println("STATE MACHINE: MANUAL_BRIDGE_OPEN_REQUESTED received - issuing RAISE_BRIDGE command");
                changeState(BridgeState::MANUAL_OPENING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
                Serial.println("STATE MACHINE: Now in MANUAL_OPENING, waiting for BRIDGE_OPENED_SUCCESS...");
            } else {
                Serial.println("STATE MACHINE: MANUAL_CLOSED state waiting for manual open command");
            }
            break;

        default:
            Serial.print("STATE MACHINE: Unknown state: ");
            Serial.println((int)m_currentState);
            break;
    }
}

void BridgeStateMachine::changeState(BridgeState newState) {
    m_previousState = m_currentState;
    m_currentState = newState;
    m_stateEntryTime = millis();
    
    Serial.print("STATE MACHINE: State changed from ");
    Serial.print((int)m_previousState);
    Serial.print(" to ");
    Serial.println((int)m_currentState);
    
    // Publish state change event for monitoring systems
    auto* stateChangeData = new StateChangeData(m_currentState, m_previousState);
    m_eventBus.publish(BridgeEvent::STATE_CHANGED, stateChangeData);
}

void BridgeStateMachine::issueCommand(CommandTarget target, CommandAction action) {
    Command cmd;
    cmd.target = target;
    cmd.action = action;
    cmd.data = "";  // Empty data for commands that don't need it
    
    Serial.print("STATE MACHINE: Issuing command - Target: ");
    Serial.print((int)target);
    Serial.print(", Action: ");
    Serial.println((int)action);
    
    m_commandBus.publish(cmd);
}

void BridgeStateMachine::issueCommand(CommandTarget target, CommandAction action, const String& data) {
    Command cmd;
    cmd.target = target;
    cmd.action = action;
    cmd.data = data;
    
    Serial.print("STATE MACHINE: Issuing command - Target: ");
    Serial.print((int)target);
    Serial.print(", Action: ");
    Serial.print((int)action);
    Serial.print(", Data: ");
    Serial.println(data);
    
    m_commandBus.publish(cmd);
}

BridgeState BridgeStateMachine::getCurrentState() const {
    return m_currentState;
}

String BridgeStateMachine::getStateString() const {
    switch (m_currentState) {
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
        default: return "UNKNOWN";
    }
}

// Unsure if this implementation is sound but i will delve into it further as the Bus's are developed.
void BridgeStateMachine::subscribeToEvents() {
    // Subscribe to all events that the state machine needs to handle
    auto eventCallback = [this](EventData* eventData) {
        this->onEventReceived(eventData);
    };

    // Subscribe to external events
    m_eventBus.subscribe(BridgeEvent::BOAT_DETECTED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSED, eventCallback, EventPriority::NORMAL);
    
    // Subscribe to manual control events (Command Mode)
    m_eventBus.subscribe(BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED, eventCallback, EventPriority::NORMAL);
    
    // Subscribe to success events from subsystems
    m_eventBus.subscribe(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BRIDGE_OPENED_SUCCESS, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BRIDGE_CLOSED_SUCCESS, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::INDICATOR_UPDATE_SUCCESS, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::SYSTEM_SAFE_SUCCESS, eventCallback, EventPriority::NORMAL);
    
    // Subscribe to safety and manual override events (high priority)
    m_eventBus.subscribe(BridgeEvent::FAULT_DETECTED, eventCallback, EventPriority::EMERGENCY);
    m_eventBus.subscribe(BridgeEvent::FAULT_CLEARED, eventCallback, EventPriority::EMERGENCY);
    m_eventBus.subscribe(BridgeEvent::MANUAL_OVERRIDE_ACTIVATED, eventCallback, EventPriority::EMERGENCY);
    m_eventBus.subscribe(BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED, eventCallback, EventPriority::EMERGENCY);
    
    Serial.println("STATE MACHINE: Subscribed to all relevant events on EventBus");
}

void BridgeStateMachine::onEventReceived(EventData* eventData) {
    if (eventData == nullptr) {
        Serial.println("STATE MACHINE: Received null event data");
        return;
    }
    
    // Extract the event type and forward to our existing handleEvent logic
    BridgeEvent event = eventData->getEventEnum();
    
    // Only log significant state changes, not every event
    handleEvent(event);
}