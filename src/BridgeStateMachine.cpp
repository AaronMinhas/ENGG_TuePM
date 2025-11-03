#include "BridgeStateMachine.h"
#include "DetectionSystem.h"
#include <Arduino.h>
#include "Logger.h"

/*
 * SIMPLIFIED BOAT DETECTION SYSTEM:
 * - One boat served at a time (no queue)
 * - Opposite ultrasonic sensor disabled during boat cycle
 * - Simple flow: IDLE -> detect boat -> stop traffic -> open bridge -> boat passes -> close -> resume traffic -> IDLE
 * - Sensors re-enabled when traffic resumes and cycle completes
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
    LOG_INFO(Logger::TAG_FSM, "Initialised and subscribed to EventBus");
}

void BridgeStateMachine::setDetectionSystem(DetectionSystem* detectionSystem) {
    m_detectionSystem = detectionSystem;
    LOG_INFO(Logger::TAG_FSM, "DetectionSystem reference set");
}

void BridgeStateMachine::handleEvent(const BridgeEvent& event) {
    // Only log important state transitions, not every event
    
    // Global event handling. Takes precedence over state specific events
    // Does not implement Safety Manager but that is low priority atp.
    if (event == BridgeEvent::SYSTEM_RESET_REQUESTED) {
        performSystemReset();
        return;
    }

    if (event == BridgeEvent::FAULT_DETECTED || event == BridgeEvent::BOAT_PASSAGE_TIMEOUT) {
        if (m_currentState != BridgeState::FAULT) {
            resetBoatCycleState();
            if (event == BridgeEvent::BOAT_PASSAGE_TIMEOUT) {
                LOG_ERROR(Logger::TAG_FSM, "BOAT_PASSAGE_TIMEOUT - boat didn't pass within timeout → FAULT state");
            } else {
                LOG_WARN(Logger::TAG_FSM, "FAULT detected → entering FAULT state");
            }
            changeState(BridgeState::FAULT);
            issueCommand(CommandTarget::CONTROLLER, CommandAction::ENTER_SAFE_STATE);
        }
        return;
    }

    if (event == BridgeEvent::MANUAL_OVERRIDE_ACTIVATED) {
        if (m_currentState != BridgeState::MANUAL_MODE) {
            LOG_INFO(Logger::TAG_FSM, "MANUAL_OVERRIDE_ACTIVATED - entering MANUAL_MODE");
            changeState(BridgeState::MANUAL_MODE);
            // No command issued - manual mode pauses the state machine
        }
        return;
    }

    if (event == BridgeEvent::BEAM_BREAK_ACTIVE) {
        // Beam break active events are handled via deferral logic in helper methods
        return;
    }

    if (event == BridgeEvent::BEAM_BREAK_CLEAR) {
        processPendingLowerRequest();
        return;
    }

    // Boat detection handling is global so requests can be queued during any state
    if (event == BridgeEvent::BOAT_DETECTED_LEFT || event == BridgeEvent::BOAT_DETECTED_RIGHT) {
        // Check if we're waiting for opposite side detection during clearance period
        if (waitingToClearBeforeClose_ && activeBoatSide_ != BoatSide::UNKNOWN) {
            BoatSide oppositeSide = (activeBoatSide_ == BoatSide::LEFT) ? BoatSide::RIGHT : BoatSide::LEFT;
            if (lastEventSide_ == oppositeSide) {
                oppositeSideDetectedDuringClearance_ = true;
                LOG_INFO(Logger::TAG_FSM, "Opposite side (%s) detected boat during clearance period - bridge will close after delay", sideName(oppositeSide));
            }
        }
        handleBoatDetection(lastEventSide_);
        return;
    }
    if (event == BridgeEvent::BOAT_DETECTED) {
        // Generic detection event kept for backward compatibility - already handled by side-specific events
        return;
    }

    // State specific event handling - states only change on SUCCESS events
    switch (m_currentState) {
        case BridgeState::IDLE:
            // IDLE state handles trigger events and success confirmations
            if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "Bridge open requested → opening");
                changeState(BridgeState::MANUAL_OPENING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
            } else if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "Bridge close requested → attempting to close");
                issueLowerBridgeManual();
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_TRAFFIC_STOP_REQUESTED in IDLE - issuing STOP_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
                LOG_INFO(Logger::TAG_FSM, "Traffic stop requested manually, staying in IDLE...");
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_TRAFFIC_RESUME_REQUESTED in IDLE - issuing RESUME_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
                LOG_INFO(Logger::TAG_FSM, "Traffic resume requested manually, staying in IDLE...");
            }
            break;

        case BridgeState::STOPPING_TRAFFIC:
            // STOPPING_TRAFFIC state waits for traffic to stop, then starts bridge opening
            if (event == BridgeEvent::TRAFFIC_STOPPED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "TRAFFIC_STOPPED_SUCCESS received - transitioning to OPENING and issuing RAISE_BRIDGE");
                changeState(BridgeState::OPENING);
                // Now issue the next command: raise bridge
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
                LOG_INFO(Logger::TAG_FSM, "Now waiting for BRIDGE_OPENED_SUCCESS...");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "STOPPING_TRAFFIC state ignoring non-success event - still waiting for TRAFFIC_STOPPED_SUCCESS");
            }
            break;

        case BridgeState::OPENING:
            // OPENING state waits for bridge to finish opening, then transitions to OPEN
            if (event == BridgeEvent::BRIDGE_OPENED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_OPENED_SUCCESS received - transitioning to OPEN");
                changeState(BridgeState::OPEN);
                
                // Record entry time for emergency timeout
                openingStateEntryTime_ = millis();
                
                // Set boat lights green for the active side
                if (activeBoatSide_ != BoatSide::UNKNOWN) {
                    String sideStr = boatSideToString(activeBoatSide_);
                    if (activeBoatSide_ == BoatSide::LEFT) {
                        issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::SET_BOAT_LIGHT_LEFT, "green");
                    } else {
                        issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::SET_BOAT_LIGHT_RIGHT, "green");
                    }
                    LOG_INFO(Logger::TAG_FSM, "Boat light set to GREEN for side=%s", sideName(activeBoatSide_));
                }
                
                // Now wait for BOAT_PASSED (beam break detects when boat clears)
                LOG_INFO(Logger::TAG_FSM, "Now waiting for BOAT_PASSED (beam break will detect when boat clears)");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "OPENING state ignoring non-success event - still waiting for BRIDGE_OPENED_SUCCESS");
            }
            break;

        case BridgeState::OPEN:
            // OPEN state waits for boat to pass
            if (event == BridgeEvent::BOAT_PASSED || 
                event == BridgeEvent::BOAT_PASSED_LEFT || 
                event == BridgeEvent::BOAT_PASSED_RIGHT) {
                // Beam break sensor detects passage
                LOG_INFO(Logger::TAG_FSM, "BOAT_PASSED detected via beam break - boat has cleared the channel");
                
                // Turn off boat lights
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::SET_BOAT_LIGHT_LEFT, "red");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::SET_BOAT_LIGHT_RIGHT, "red");
                
                // Re-enable opposite sensor to detect boat on the other side
                if (m_detectionSystem != nullptr && activeBoatSide_ != BoatSide::UNKNOWN) {
                    DetectionSystem::BoatDirection direction = (activeBoatSide_ == BoatSide::LEFT) 
                        ? DetectionSystem::BoatDirection::LEFT_TO_RIGHT 
                        : DetectionSystem::BoatDirection::RIGHT_TO_LEFT;
                    m_detectionSystem->enableOppositeSensor(direction);
                    m_detectionSystem->resetBoatDetectionState();  // Allow opposite sensor to trigger new detection
                }
                
                // Start clearance delay timer before closing bridge
                boatClearanceTime_ = millis();
                waitingToClearBeforeClose_ = true;
                oppositeSideDetectedDuringClearance_ = false;  // Reset opposite side detection tracking
                LOG_INFO(Logger::TAG_FSM, "Starting 10-second clearance delay - monitoring opposite side for boat detection");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "OPEN state ignoring non-relevant event - still waiting for BOAT_PASSED");
            }
            break;

        case BridgeState::CLOSING:
            // CLOSING state waits for bridge to finish closing, then resumes traffic
            if (event == BridgeEvent::BRIDGE_CLOSED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_CLOSED_SUCCESS received - transitioning to RESUMING_TRAFFIC");
                changeState(BridgeState::RESUMING_TRAFFIC);
                // Issue command to resume traffic
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
                LOG_INFO(Logger::TAG_FSM, "Now waiting for TRAFFIC_RESUMED_SUCCESS...");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "CLOSING state ignoring non-success event - still waiting for BRIDGE_CLOSED_SUCCESS");
            }
            break;

        case BridgeState::RESUMING_TRAFFIC:
            // RESUMING_TRAFFIC state waits for traffic to be confirmed resumed
            if (event == BridgeEvent::TRAFFIC_RESUMED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "TRAFFIC_RESUMED_SUCCESS received - returning to IDLE");
                completeBridgeCycle();
                changeState(BridgeState::IDLE);
                LOG_INFO(Logger::TAG_FSM, "Bridge operation complete - ready for next boat");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "RESUMING_TRAFFIC state ignoring non-success event - still waiting for TRAFFIC_RESUMED_SUCCESS");
            }
            break;

        case BridgeState::FAULT:
            // FAULT state waits ONLY for fault to be cleared
            if (event == BridgeEvent::FAULT_CLEARED) {
                LOG_INFO(Logger::TAG_FSM, "FAULT_CLEARED received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // System should be reset to safe state (bridge closed, traffic flowing)
                LOG_INFO(Logger::TAG_FSM, "Fault cleared - system should be in safe state");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "FAULT state ignoring non-clear event - still waiting for FAULT_CLEARED");
            }
            break;

        case BridgeState::MANUAL_MODE:
            // MANUAL_MODE waits ONLY for manual override to be deactivated
            if (event == BridgeEvent::MANUAL_OVERRIDE_DEACTIVATED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_OVERRIDE_DEACTIVATED received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // Assume operator has returned system to safe state
                LOG_INFO(Logger::TAG_FSM, "Manual mode deactivated - assuming system in safe state");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "MANUAL_MODE ignoring operational events - still in manual mode");
            }
            // In manual mode, the state machine doesn't process normal operational events
            break;

        case BridgeState::MANUAL_OPENING:
            // MANUAL_OPENING state waits for bridge to finish opening
            if (event == BridgeEvent::BRIDGE_OPENED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_OPENED_SUCCESS received - transitioning to MANUAL_OPEN");
                changeState(BridgeState::MANUAL_OPEN);
                LOG_INFO(Logger::TAG_FSM, "Bridge manually opened - waiting for manual close command");
            } else if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_BRIDGE_CLOSE_REQUESTED while opening - will close after opening completes");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "MANUAL_OPENING state ignoring non-success event - still waiting for BRIDGE_OPENED_SUCCESS");
            }
            break;

        case BridgeState::MANUAL_OPEN:
            // MANUAL_OPEN state waits for manual close command
            if (event == BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_BRIDGE_CLOSE_REQUESTED received - attempting to lower bridge");
                issueLowerBridgeManual();
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_STOP_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_TRAFFIC_STOP_REQUESTED while bridge open - issuing STOP_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
            } else if (event == BridgeEvent::MANUAL_TRAFFIC_RESUME_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_TRAFFIC_RESUME_REQUESTED while bridge open - issuing RESUME_TRAFFIC command");
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "MANUAL_OPEN state waiting for manual close command");
            }
            break;

        case BridgeState::MANUAL_CLOSING:
            // MANUAL_CLOSING state waits for bridge to finish closing
            if (event == BridgeEvent::BRIDGE_CLOSED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_CLOSED_SUCCESS received - returning to IDLE");
                changeState(BridgeState::IDLE);
                LOG_INFO(Logger::TAG_FSM, "Bridge manually closed - back to IDLE state");
            } else if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_BRIDGE_OPEN_REQUESTED while closing - will open after closing completes");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "MANUAL_CLOSING state ignoring non-success event - still waiting for BRIDGE_CLOSED_SUCCESS");
            }
            break;

        case BridgeState::MANUAL_CLOSED:
            // MANUAL_CLOSED state waits for manual open command (unused at the momement)
            if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "MANUAL_BRIDGE_OPEN_REQUESTED received - issuing RAISE_BRIDGE command");
                changeState(BridgeState::MANUAL_OPENING);
                issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
                LOG_INFO(Logger::TAG_FSM, "Now in MANUAL_OPENING, waiting for BRIDGE_OPENED_SUCCESS...");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "MANUAL_CLOSED state waiting for manual open command");
            }
            break;

        default:
            LOG_ERROR(Logger::TAG_FSM, "Unknown state: %d", static_cast<int>(m_currentState));
            break;
    }
}

void BridgeStateMachine::handleBoatDetection(BoatSide side) {
    if (side == BoatSide::UNKNOWN) {
        LOG_WARN(Logger::TAG_FSM, "Boat detected but side unknown - ignoring");
        return;
    }

    // Ignore if a boat cycle is already active
    if (boatCycleActive_) {
        LOG_INFO(Logger::TAG_FSM, "Boat detected on side=%s but cycle already active - ignoring (opposite sensor will be disabled)",
                 sideName(side));
        return;
    }

    // Only start cycle if in IDLE state
    if (m_currentState != BridgeState::IDLE) {
        LOG_INFO(Logger::TAG_FSM, "Boat detected on side=%s but bridge not in IDLE state (%s) - ignoring",
                 sideName(side), stateName(m_currentState));
        return;
    }

    // Start bridge cycle for this boat
    beginBridgeCycle(side);
}

void BridgeStateMachine::beginBridgeCycle(BoatSide side) {
    boatCycleActive_ = true;
    activeBoatSide_ = side;

    LOG_INFO(Logger::TAG_FSM, "Starting bridge cycle for boat from side=%s - transitioning to STOPPING_TRAFFIC", sideName(side));
    changeState(BridgeState::STOPPING_TRAFFIC);
    issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
    LOG_INFO(Logger::TAG_FSM, "Now waiting for TRAFFIC_STOPPED_SUCCESS...");
}

void BridgeStateMachine::completeBridgeCycle() {
    // Re-enable all sensors for next detection
    if (m_detectionSystem != nullptr) {
        m_detectionSystem->enableAllSensors();
    }
    
    // Reset boat cycle tracking
    activeBoatSide_ = BoatSide::UNKNOWN;
    boatCycleActive_ = false;
    openingStateEntryTime_ = 0;
    boatClearanceTime_ = 0;
    waitingToClearBeforeClose_ = false;
    oppositeSideDetectedDuringClearance_ = false;
    
    LOG_INFO(Logger::TAG_FSM, "Bridge cycle complete - sensors re-enabled and ready for next boat");
}


String BridgeStateMachine::boatSideToString(BoatSide side) {
    switch (side) {
        case BoatSide::LEFT:
            return String("left");
        case BoatSide::RIGHT:
            return String("right");
        default:
            return String("unknown");
    }
}

bool BridgeStateMachine::issueLowerBridgeAuto() {
    if (beamBreakActive_) {
        if (pendingLowerRequest_ != PendingLowerRequest::AUTO) {
            pendingLowerRequest_ = PendingLowerRequest::AUTO;
            LOG_WARN(Logger::TAG_FSM, "Beam break active - deferring automatic bridge lowering until channel is clear");
        }
        return false;
    }

    pendingLowerRequest_ = PendingLowerRequest::NONE;
    changeState(BridgeState::CLOSING);
    issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::LOWER_BRIDGE);
    LOG_INFO(Logger::TAG_FSM, "Lowering bridge for traffic - waiting for BRIDGE_CLOSED_SUCCESS...");
    return true;
}

bool BridgeStateMachine::issueLowerBridgeManual() {
    if (beamBreakActive_) {
        if (pendingLowerRequest_ != PendingLowerRequest::MANUAL) {
            pendingLowerRequest_ = PendingLowerRequest::MANUAL;
            LOG_WARN(Logger::TAG_FSM, "Beam break active - manual bridge closing deferred until beam clears");
        }
        return false;
    }

    pendingLowerRequest_ = PendingLowerRequest::NONE;
    changeState(BridgeState::MANUAL_CLOSING);
    issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::LOWER_BRIDGE);
    LOG_INFO(Logger::TAG_FSM, "Manual bridge close in progress - waiting for BRIDGE_CLOSED_SUCCESS...");
    return true;
}

void BridgeStateMachine::processPendingLowerRequest() {
    if (pendingLowerRequest_ == PendingLowerRequest::NONE) {
        LOG_DEBUG(Logger::TAG_FSM, "Beam break cleared with no pending lower request");
        return;
    }

    PendingLowerRequest requested = pendingLowerRequest_;
    if (requested == PendingLowerRequest::AUTO) {
        if (!issueLowerBridgeAuto()) {
            return;
        }
        LOG_INFO(Logger::TAG_FSM, "Beam break cleared - resuming automatic bridge lowering");
    } else if (requested == PendingLowerRequest::MANUAL) {
        if (!issueLowerBridgeManual()) {
            return;
        }
        LOG_INFO(Logger::TAG_FSM, "Beam break cleared - resuming manual bridge lowering");
    }
}

void BridgeStateMachine::resetBoatCycleState() {
    // Reset all boat cycle state
    activeBoatSide_ = BoatSide::UNKNOWN;
    lastEventSide_ = BoatSide::UNKNOWN;
    boatCycleActive_ = false;
    openingStateEntryTime_ = 0;
    boatClearanceTime_ = 0;
    waitingToClearBeforeClose_ = false;
    oppositeSideDetectedDuringClearance_ = false;
    pendingLowerRequest_ = PendingLowerRequest::NONE;
    beamBreakActive_ = false;
    
    // Re-enable all sensors
    if (m_detectionSystem != nullptr) {
        m_detectionSystem->enableAllSensors();
    }
    
    LOG_INFO(Logger::TAG_FSM, "Boat cycle state reset - sensors re-enabled");
}

void BridgeStateMachine::performSystemReset() {
    LOG_WARN(Logger::TAG_FSM, "SYSTEM_RESET_REQUESTED - forcing system back to IDLE");

    resetBoatCycleState();

    if (m_currentState != BridgeState::IDLE) {
        changeState(BridgeState::IDLE);
    } else {
        auto* stateChangeData = new StateChangeData(BridgeState::IDLE, m_previousState);
        m_eventBus.publish(BridgeEvent::STATE_CHANGED, stateChangeData);
    }

    issueCommand(CommandTarget::CONTROLLER, CommandAction::RESET_TO_IDLE_STATE);
}

void BridgeStateMachine::changeState(BridgeState newState) {
    m_previousState = m_currentState;
    m_currentState = newState;
    m_stateEntryTime = millis();
    
    LOG_INFO(Logger::TAG_FSM, "State changed from %s to %s",
             stateName(m_previousState), stateName(m_currentState));
    
    // Publish state change event for monitoring systems
    auto* stateChangeData = new StateChangeData(m_currentState, m_previousState);
    m_eventBus.publish(BridgeEvent::STATE_CHANGED, stateChangeData);
}

void BridgeStateMachine::issueCommand(CommandTarget target, CommandAction action) {
    Command cmd;
    cmd.target = target;
    cmd.action = action;
    cmd.data = "";  // Empty data for commands that don't need it
    
    LOG_DEBUG(Logger::TAG_FSM, "Issuing command - Target: %d, Action: %d",
              static_cast<int>(target), static_cast<int>(action));
    
    m_commandBus.publish(cmd);
}

void BridgeStateMachine::issueCommand(CommandTarget target, CommandAction action, const String& data) {
    Command cmd;
    cmd.target = target;
    cmd.action = action;
    cmd.data = data;
    
    LOG_DEBUG(Logger::TAG_FSM, "Issuing command - Target: %d, Action: %d, Data: %s",
              static_cast<int>(target), static_cast<int>(action), data.c_str());
    
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

const char* BridgeStateMachine::stateName(BridgeState s) {
    switch (s) {
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
    m_eventBus.subscribe(BridgeEvent::BOAT_DETECTED_LEFT, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_DETECTED_RIGHT, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSED_LEFT, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSED_RIGHT, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BEAM_BREAK_ACTIVE, eventCallback, EventPriority::EMERGENCY);
    m_eventBus.subscribe(BridgeEvent::BEAM_BREAK_CLEAR, eventCallback, EventPriority::EMERGENCY);
    
    // Subscribe to boat queue events
    m_eventBus.subscribe(BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED, eventCallback, EventPriority::NORMAL);
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSAGE_TIMEOUT, eventCallback, EventPriority::EMERGENCY);
    
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
    m_eventBus.subscribe(BridgeEvent::SYSTEM_RESET_REQUESTED, eventCallback, EventPriority::EMERGENCY);
    
    LOG_INFO(Logger::TAG_FSM, "Subscribed to all relevant events on EventBus");
}

void BridgeStateMachine::checkTimeouts() {
    // Check for emergency timeout in OPEN state
    if (m_currentState == BridgeState::OPEN && openingStateEntryTime_ > 0) {
        unsigned long elapsed = millis() - openingStateEntryTime_;
        
        if (elapsed >= BOAT_PASSAGE_TIMEOUT_MS) {
            LOG_ERROR(Logger::TAG_FSM, "Emergency timeout in OPEN state (%lu ms) - boat didn't pass", elapsed);
            
            // Publish timeout event (will trigger FAULT via global handler)
            auto* timeoutData = new SimpleEventData(BridgeEvent::BOAT_PASSAGE_TIMEOUT);
            m_eventBus.publish(BridgeEvent::BOAT_PASSAGE_TIMEOUT, timeoutData, EventPriority::EMERGENCY);
        }
    }
    
    // Check for clearance delay completion before closing bridge
    if (waitingToClearBeforeClose_ && boatClearanceTime_ > 0) {
        unsigned long elapsed = millis() - boatClearanceTime_;
        
        if (elapsed >= BOAT_CLEARANCE_DELAY_MS) {
            LOG_INFO(Logger::TAG_FSM, "Clearance delay complete (%lu ms)", elapsed);
            
            // Only close bridge if opposite side detected a boat during the clearance period
            if (oppositeSideDetectedDuringClearance_) {
                LOG_INFO(Logger::TAG_FSM, "Opposite side detected during clearance - proceeding to close bridge");
                waitingToClearBeforeClose_ = false;
                boatClearanceTime_ = 0;
                oppositeSideDetectedDuringClearance_ = false;
                
                // Now attempt to lower bridge
                issueLowerBridgeAuto();
            } else {
                LOG_ERROR(Logger::TAG_FSM, "Opposite side did NOT detect boat during %lu ms clearance period - entering FAULT state", elapsed);
                
                // Reset clearance tracking
                waitingToClearBeforeClose_ = false;
                boatClearanceTime_ = 0;
                oppositeSideDetectedDuringClearance_ = false;
                
                // Trigger fault - boat passed beam break but never reached opposite sensor
                auto* faultData = new SimpleEventData(BridgeEvent::FAULT_DETECTED);
                m_eventBus.publish(BridgeEvent::FAULT_DETECTED, faultData, EventPriority::EMERGENCY);
            }
        }
    }
}

void BridgeStateMachine::onEventReceived(EventData* eventData) {
    if (eventData == nullptr) {
        LOG_WARN(Logger::TAG_FSM, "Received null event data");
        return;
    }
    
    // Extract the event type and forward to our existing handleEvent logic
    BridgeEvent event = eventData->getEventEnum();
    if (event == BridgeEvent::BEAM_BREAK_ACTIVE) {
        beamBreakActive_ = true;
    } else if (event == BridgeEvent::BEAM_BREAK_CLEAR) {
        beamBreakActive_ = false;
    }

    const BoatEventSide sideInfo = eventData->getBoatEventSide();
    if (sideInfo == BoatEventSide::LEFT || sideInfo == BoatEventSide::RIGHT) {
        BoatSide parsedSide = (sideInfo == BoatEventSide::LEFT) ? BoatSide::LEFT : BoatSide::RIGHT;
        lastEventSide_ = parsedSide;
        if ((event == BridgeEvent::BOAT_DETECTED || 
             event == BridgeEvent::BOAT_DETECTED_LEFT || 
             event == BridgeEvent::BOAT_DETECTED_RIGHT) && !boatCycleActive_) {
            activeBoatSide_ = parsedSide;
        }
    }
    
    // Only log significant state changes, not every event
    handleEvent(event);
}
