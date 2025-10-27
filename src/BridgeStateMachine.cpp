#include "BridgeStateMachine.h"
#include <Arduino.h>
#include "Logger.h"
#include "TrafficCounter.h"

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

void BridgeStateMachine::setTrafficCounter(TrafficCounter* counter) {
    trafficCounter_ = counter;
    if (trafficCounter_ != nullptr) {
        lastKnownLeftTrafficCount_ = trafficCounter_->getLeftCount();
        lastKnownRightTrafficCount_ = trafficCounter_->getRightCount();
    } else {
        lastKnownLeftTrafficCount_ = 0;
        lastKnownRightTrafficCount_ = 0;
    }
}

void BridgeStateMachine::begin() {
    changeState(BridgeState::IDLE);
    subscribeToEvents();
    LOG_INFO(Logger::TAG_FSM, "Initialised and subscribed to EventBus");
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
            resetBoatCycleState(true);
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
            if (event == BridgeEvent::TRAFFIC_STOPPED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "TRAFFIC_STOPPED_SUCCESS received - evaluating traffic counts before opening");
                changeState(BridgeState::STOPPING_TRAFFIC);
                pendingOpenAction_ = PendingOpenAction::AUTOMATIC;
                if (shouldDelayForTrafficBeforeOpening()) {
                    LOG_INFO(Logger::TAG_FSM,
                             "Waiting for vehicle count to clear before raising bridge (left=%d, right=%d)",
                             lastKnownLeftTrafficCount_, lastKnownRightTrafficCount_);
                } else {
                    tryBeginPendingBridgeOpening("traffic stop confirmed");
                }
            } else if (event == BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED) {
                LOG_INFO(Logger::TAG_FSM, "Manual bridge open requested");
                pendingOpenAction_ = PendingOpenAction::MANUAL;
                if (shouldDelayForManualOpening()) {
                    LOG_INFO(Logger::TAG_FSM,
                             "Manual open will begin once vehicle count reaches zero (left=%d, right=%d)",
                             lastKnownLeftTrafficCount_, lastKnownRightTrafficCount_);
                } else {
                    tryBeginPendingBridgeOpening("manual request");
                }
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
            // STOPPING_TRAFFIC state waits ONLY for bridge to be confirmed fully open
            if (event == BridgeEvent::BRIDGE_OPENED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_OPENED_SUCCESS received - transitioning to OPENING");
                changeState(BridgeState::OPENING);
                
                // Record entry time for emergency timeout
                openingStateEntryTime_ = millis();
                
                if (activeBoatSide_ == BoatSide::UNKNOWN) {
                    if (hasPendingBoatRequests()) {
                        // Edge case: active side lost but queue still populated
                        BoatSide recovered = boatQueue_.front();
                        boatQueue_.pop_front();
                        activeBoatSide_ = recovered;
                    } else {
                        LOG_WARN(Logger::TAG_FSM, "Bridge opened but no active boat side - keeping lights red");
                    }
                }

                if (activeBoatSide_ != BoatSide::UNKNOWN) {
                    startActiveBoatWindow(activeBoatSide_);
                }
                
                // Now wait for BOAT_PASSED (opposite side to clear)
                LOG_INFO(Logger::TAG_FSM, "Now waiting for BOAT_PASSED on side=%s",
                         sideName(otherSide(activeBoatSide_)));
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "STOPPING_TRAFFIC state ignoring non-success event - still waiting for BRIDGE_OPENED_SUCCESS");
            }
            break;

        case BridgeState::OPENING:
            // OPENING state waits for boat to pass (and handles green period expiration)
            if (event == BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED) {
                endActiveBoatWindow("timer expired");
            } else if (event == BridgeEvent::BOAT_PASSED || 
                       event == BridgeEvent::BOAT_PASSED_LEFT || 
                       event == BridgeEvent::BOAT_PASSED_RIGHT) {
                BoatSide expectedExit = otherSide(activeBoatSide_);
                if (expectedExit == BoatSide::UNKNOWN) {
                    LOG_WARN(Logger::TAG_FSM, "BOAT_PASSED received but active side unknown - ignoring");
                } else if (lastEventSide_ == BoatSide::UNKNOWN) {
                    LOG_WARN(Logger::TAG_FSM, "BOAT_PASSED received without side info - ignoring");
                } else if (lastEventSide_ != expectedExit) {
                    LOG_WARN(Logger::TAG_FSM, "BOAT_PASSED on unexpected side=%s (expected %s) - ignoring",
                             sideName(lastEventSide_), sideName(expectedExit));
                } else {
                    boatPassedInWindow_ = true;
                    LOG_INFO(Logger::TAG_FSM, "BOAT_PASSED verified on expected side=%s - continuing window for remaining time",
                             sideName(lastEventSide_));
                }
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "OPENING state ignoring non-relevant event - still waiting for BOAT_PASSED");
            }
            break;

        case BridgeState::OPEN:
            // OPEN state waits ONLY for bridge to be confirmed fully closed
            if (event == BridgeEvent::BRIDGE_CLOSED_SUCCESS) {
                LOG_INFO(Logger::TAG_FSM, "BRIDGE_CLOSED_SUCCESS received - transitioning to CLOSING");
                changeState(BridgeState::CLOSING);
                // Issue command to resume traffic
                issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::RESUME_TRAFFIC);
                LOG_INFO(Logger::TAG_FSM, "Now waiting for TRAFFIC_RESUMED_SUCCESS...");
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "OPEN state ignoring non-success event - still waiting for BRIDGE_CLOSED_SUCCESS");
            }
            break;

        case BridgeState::CLOSING:
            // CLOSING state waits ONLY for traffic to be confirmed resumed
            if (event == BridgeEvent::TRAFFIC_RESUMED_SUCCESS) {
                startCooldown();
                LOG_INFO(Logger::TAG_FSM, "TRAFFIC_RESUMED_SUCCESS received - returning to IDLE");
                changeState(BridgeState::IDLE);
                // No entry action - back to idle, ready for next boat
                LOG_INFO(Logger::TAG_FSM, "Bridge operation complete - ready for next boat");
                // Reset boat cycle tracking (allow for new detections)
                activeBoatSide_ = BoatSide::UNKNOWN;
                boatCycleActive_ = false;
                greenWindowActive_ = false;
                sidesServedThisOpening_ = 0;
                if (hasPendingBoatRequests()) {
                    LOG_INFO(Logger::TAG_FSM, "Pending boat requests in queue (%u) - waiting for cooldown before next cycle",
                             static_cast<unsigned int>(boatQueue_.size()));
                }
            } else {
                LOG_DEBUG(Logger::TAG_FSM, "CLOSING state ignoring non-success event - still waiting for TRAFFIC_RESUMED_SUCCESS");
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

    // Ignore duplicate detections for the side currently holding the green window
    if (boatCycleActive_ && greenWindowActive_ && side == activeBoatSide_) {
        LOG_DEBUG(Logger::TAG_FSM, "Boat detected on active side=%s while window is green - already being served",
                  sideName(side));
        return;
    }

    boatQueue_.push_back(side);
    LOG_INFO(Logger::TAG_FSM, "Queued boat request for side=%s (queue length=%u)",
             sideName(side), static_cast<unsigned int>(boatQueue_.size()));

    if (maybeStartPendingCycle()) {
        return;
    }

    if (boatCycleActive_) {
        LOG_INFO(Logger::TAG_FSM, "Boat cycle already active - request will be served in FIFO order when current cycle completes");
    } else if (!canStartNewCycle()) {
        unsigned long remaining = BOAT_CYCLE_COOLDOWN_MS;
        if (cooldownActive_) {
            unsigned long elapsed = millis() - cooldownStartTime_;
            if (elapsed < BOAT_CYCLE_COOLDOWN_MS) {
                remaining = BOAT_CYCLE_COOLDOWN_MS - elapsed;
            } else {
                remaining = 0;
            }
        }
        LOG_INFO(Logger::TAG_FSM, "Bridge cooldown active (%lums remaining) - boat request queued until bridge ready",
                 remaining);
    } else if (m_currentState != BridgeState::IDLE) {
        LOG_INFO(Logger::TAG_FSM, "Bridge state %s not ready to begin cycle yet - boat request queued",
                 stateName(m_currentState));
    }
}

bool BridgeStateMachine::maybeStartPendingCycle() {
    if (boatQueue_.empty()) {
        return false;
    }
    if (boatCycleActive_) {
        return false;
    }
    if (!canStartNewCycle()) {
        return false;
    }
    if (m_currentState != BridgeState::IDLE) {
        return false;
    }

    BoatSide nextSide = boatQueue_.front();
    boatQueue_.pop_front();
    beginCycleForSide(nextSide);
    return true;
}

void BridgeStateMachine::beginCycleForSide(BoatSide side) {
    boatCycleActive_ = true;
    activeBoatSide_ = side;
    sidesServedThisOpening_ = 0;
    greenWindowActive_ = false;
    boatPassedInWindow_ = false;
    resetCooldown();

    LOG_INFO(Logger::TAG_FSM, "Starting boat cycle for side=%s - issuing STOP_TRAFFIC", sideName(side));
    issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::STOP_TRAFFIC);
    LOG_INFO(Logger::TAG_FSM, "Staying in IDLE, waiting for TRAFFIC_STOPPED_SUCCESS...");
}

void BridgeStateMachine::startActiveBoatWindow(BoatSide side) {
    if (side == BoatSide::UNKNOWN) {
        LOG_WARN(Logger::TAG_FSM, "Cannot start boat window - side unknown");
        return;
    }

    String sideStr = boatSideToString(side);
    greenWindowActive_ = true;
    openingStateEntryTime_ = millis();
    boatPassedInWindow_ = false;
    issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::START_BOAT_GREEN_PERIOD, sideStr);

    LOG_INFO(Logger::TAG_FSM, "Started boat queue window: %s=GREEN, %s=RED (45s timer)",
             sideName(side), sideName(otherSide(side)));
}

void BridgeStateMachine::endActiveBoatWindow(const char* reason) {
    if (!greenWindowActive_) {
        LOG_DEBUG(Logger::TAG_FSM, "endActiveBoatWindow(%s) called but no active window", reason);
        return;
    }

    BoatSide finishingSide = activeBoatSide_;

    if (!boatPassedInWindow_) {
        LOG_ERROR(Logger::TAG_FSM, "Boat window expired without BOAT_PASSED confirmation (%s) - triggering fault",
                  sideName(finishingSide));
        auto* timeoutData = new SimpleEventData(BridgeEvent::BOAT_PASSAGE_TIMEOUT);
        m_eventBus.publish(BridgeEvent::BOAT_PASSAGE_TIMEOUT, timeoutData, EventPriority::EMERGENCY);
        return;
    }

    LOG_INFO(Logger::TAG_FSM, "Boat window complete (%s) for side=%s", reason, sideName(finishingSide));

    issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::END_BOAT_GREEN_PERIOD);
    greenWindowActive_ = false;
    openingStateEntryTime_ = 0;

    sidesServedThisOpening_++;
    activeBoatSide_ = BoatSide::UNKNOWN;
    boatPassedInWindow_ = false;

    if (!boatQueue_.empty() && sidesServedThisOpening_ < MAX_SIDES_PER_OPEN) {
        BoatSide nextSide = boatQueue_.front();
        boatQueue_.pop_front();
        activeBoatSide_ = nextSide;
        LOG_INFO(Logger::TAG_FSM, "Switching bridge access to queued side=%s without lowering span", sideName(nextSide));
        startActiveBoatWindow(nextSide);
        return;
    }

    if (!boatQueue_.empty()) {
        LOG_INFO(Logger::TAG_FSM, "Additional boat requests pending but closing bridge after %u sides served this opening",
                 static_cast<unsigned int>(sidesServedThisOpening_));
    }

    LOG_INFO(Logger::TAG_FSM, "All scheduled boats served - requesting bridge lower");
    issueLowerBridgeAuto();
}

bool BridgeStateMachine::canStartNewCycle() const {
    if (!cooldownActive_) {
        return true;
    }
    return cooldownElapsed();
}

bool BridgeStateMachine::cooldownElapsed() const {
    if (!cooldownActive_) {
        return true;
    }
    unsigned long elapsed = millis() - cooldownStartTime_;
    return elapsed >= BOAT_CYCLE_COOLDOWN_MS;
}

void BridgeStateMachine::startCooldown() {
    cooldownActive_ = true;
    cooldownStartTime_ = millis();
    LOG_INFO(Logger::TAG_FSM, "Bridge cooldown started (45s buffer before next cycle)");
}

void BridgeStateMachine::resetCooldown() {
    if (cooldownActive_) {
        LOG_DEBUG(Logger::TAG_FSM, "Cooldown reset - bridge ready for next cycle");
    }
    cooldownActive_ = false;
    cooldownStartTime_ = 0;
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
    changeState(BridgeState::OPEN);
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

bool BridgeStateMachine::refreshTrafficCounts() {
    if (trafficCounter_ == nullptr) {
        lastKnownLeftTrafficCount_ = 0;
        lastKnownRightTrafficCount_ = 0;
        return false;
    }
    lastKnownLeftTrafficCount_ = trafficCounter_->getLeftCount();
    lastKnownRightTrafficCount_ = trafficCounter_->getRightCount();
    return true;
}

bool BridgeStateMachine::shouldDelayForTrafficBeforeOpening() {
    if (!refreshTrafficCounts()) {
        return false;
    }

    if ((lastKnownLeftTrafficCount_ + lastKnownRightTrafficCount_) == 0) {
        return false;
    }

    LOG_WARN(Logger::TAG_FSM,
             "Automatic opening delayed - vehicles detected near span (left=%d, right=%d)",
             lastKnownLeftTrafficCount_, lastKnownRightTrafficCount_);
    return true;
}

bool BridgeStateMachine::shouldDelayForManualOpening() {
    if (!refreshTrafficCounts()) {
        return false;
    }

    if ((lastKnownLeftTrafficCount_ + lastKnownRightTrafficCount_) == 0) {
        return false;
    }

    LOG_WARN(Logger::TAG_FSM,
             "Manual opening blocked - vehicles detected near span (left=%d, right=%d)",
             lastKnownLeftTrafficCount_, lastKnownRightTrafficCount_);
    return true;
}

void BridgeStateMachine::tryBeginPendingBridgeOpening(const char* triggerSource) {
    if (pendingOpenAction_ == PendingOpenAction::NONE) {
        return;
    }

    const bool haveCounters = refreshTrafficCounts();
    if (haveCounters && (lastKnownLeftTrafficCount_ + lastKnownRightTrafficCount_) > 0) {
        LOG_DEBUG(Logger::TAG_FSM,
                  "Still waiting for zero vehicle count before opening (left=%d, right=%d)",
                  lastKnownLeftTrafficCount_, lastKnownRightTrafficCount_);
        return;
    }

    switch (pendingOpenAction_) {
        case PendingOpenAction::AUTOMATIC:
            LOG_INFO(Logger::TAG_FSM,
                     "Traffic clear (%s) - raising bridge for queued boats", triggerSource);
            issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
            LOG_INFO(Logger::TAG_FSM, "Now waiting for BRIDGE_OPENED_SUCCESS...");
            break;
        case PendingOpenAction::MANUAL:
            LOG_INFO(Logger::TAG_FSM,
                     "Traffic clear (%s) - starting manual bridge opening", triggerSource);
            changeState(BridgeState::MANUAL_OPENING);
            issueCommand(CommandTarget::MOTOR_CONTROL, CommandAction::RAISE_BRIDGE);
            LOG_INFO(Logger::TAG_FSM, "Manual opening in progress - waiting for BRIDGE_OPENED_SUCCESS...");
            break;
        case PendingOpenAction::NONE:
        default:
            break;
    }

    pendingOpenAction_ = PendingOpenAction::NONE;
}

void BridgeStateMachine::resetBoatCycleState(bool clearQueue) {
    if (greenWindowActive_) {
        issueCommand(CommandTarget::SIGNAL_CONTROL, CommandAction::END_BOAT_GREEN_PERIOD);
    }
    activeBoatSide_ = BoatSide::UNKNOWN;
    lastEventSide_ = BoatSide::UNKNOWN;
    boatCycleActive_ = false;
    greenWindowActive_ = false;
    boatPassedInWindow_ = false;
    sidesServedThisOpening_ = 0;
    openingStateEntryTime_ = 0;
    pendingLowerRequest_ = PendingLowerRequest::NONE;
    beamBreakActive_ = false;
    if (clearQueue) {
        boatQueue_.clear();
    }
    cooldownActive_ = false;
    cooldownStartTime_ = 0;
    pendingOpenAction_ = PendingOpenAction::NONE;
}

void BridgeStateMachine::performSystemReset() {
    LOG_WARN(Logger::TAG_FSM, "SYSTEM_RESET_REQUESTED - forcing system back to IDLE");

    resetBoatCycleState(true);
    pendingOpenAction_ = PendingOpenAction::NONE;

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

    if (m_currentState == BridgeState::IDLE && !boatCycleActive_) {
        maybeStartPendingCycle();
    }
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
    m_eventBus.subscribe(BridgeEvent::TRAFFIC_COUNT_CHANGED, eventCallback, EventPriority::NORMAL);

    LOG_INFO(Logger::TAG_FSM, "Subscribed to all relevant events on EventBus");
}

void BridgeStateMachine::checkTimeouts() {
    // Check for emergency timeout in OPENING state
    if (m_currentState == BridgeState::OPENING && openingStateEntryTime_ > 0) {
        unsigned long elapsed = millis() - openingStateEntryTime_;
        
        if (elapsed >= BOAT_PASSAGE_TIMEOUT_MS) {
            LOG_ERROR(Logger::TAG_FSM, "Emergency timeout in OPENING state (%lu ms) - boat didn't pass", elapsed);
            
            // Publish timeout event (will trigger FAULT via global handler)
            auto* timeoutData = new SimpleEventData(BridgeEvent::BOAT_PASSAGE_TIMEOUT);
            m_eventBus.publish(BridgeEvent::BOAT_PASSAGE_TIMEOUT, timeoutData, EventPriority::EMERGENCY);
        }
    }

    if (cooldownActive_ && cooldownElapsed()) {
        LOG_INFO(Logger::TAG_FSM, "Bridge cooldown elapsed - ready for next cycle");
        resetCooldown();
        if (m_currentState == BridgeState::IDLE) {
            maybeStartPendingCycle();
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
    } else if (event == BridgeEvent::TRAFFIC_COUNT_CHANGED) {
        auto* trafficData = static_cast<TrafficCountEventData*>(eventData);
        if (trafficData != nullptr) {
            lastKnownLeftTrafficCount_ = trafficData->getLeftCount();
            lastKnownRightTrafficCount_ = trafficData->getRightCount();
            if (pendingOpenAction_ != PendingOpenAction::NONE &&
                (trafficData->getLeftCount() + trafficData->getRightCount()) == 0) {
                tryBeginPendingBridgeOpening("vehicle count cleared");
            }
        }
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
