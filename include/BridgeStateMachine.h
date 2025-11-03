#pragma once

#include "BridgeSystemDefs.h"
#include "CommandBus.h"
#include "EventBus.h"
#include <Arduino.h>

// Forward declaration
class DetectionSystem;

class BridgeStateMachine {
public:
    BridgeStateMachine(EventBus& eventBus, CommandBus& commandBus);
    void begin();
    void handleEvent(const BridgeEvent& event);
    void checkTimeouts();  // Check for emergency timeouts
    BridgeState getCurrentState() const;
    String getStateString() const;
    
    // Set reference to detection system for sensor control
    void setDetectionSystem(DetectionSystem* detectionSystem);
    
    static const char* stateName(BridgeState s);

private:
    // Boat passage tracking (single boat at a time - no queue)
    enum class BoatSide { UNKNOWN, LEFT, RIGHT };
    BoatSide activeBoatSide_ = BoatSide::UNKNOWN;  // Side that detected the boat
    BoatSide lastEventSide_ = BoatSide::UNKNOWN;   // Side parsed from the most recent boat event
    bool boatCycleActive_ = false;                 // Set to True from first detection until traffic resumes
    
    // Timing tracking
    unsigned long openingStateEntryTime_ = 0;      // When bridge entered OPEN state
    unsigned long boatClearanceTime_ = 0;          // When boat cleared (beam break) - for delay before closing
    bool waitingToClearBeforeClose_ = false;       // Waiting for clearance delay before closing
    bool oppositeSideDetectedDuringClearance_ = false;  // Track if opposite ultrasonic detected during clearance period

    static const char* sideName(BoatSide s) {
        switch (s) { case BoatSide::LEFT: return "left"; case BoatSide::RIGHT: return "right"; default: return "unknown"; }
    }

    void changeState(BridgeState newState);
    void issueCommand(CommandTarget target, CommandAction action);
    void issueCommand(CommandTarget target, CommandAction action, const String& data);
    void subscribeToEvents();
    
    void onEventReceived(EventData* eventData);
    void handleBoatDetection(BoatSide side);
    void beginBridgeCycle(BoatSide side);
    void completeBridgeCycle();
    static String boatSideToString(BoatSide side);

    EventBus& m_eventBus;
    CommandBus& m_commandBus;
    DetectionSystem* m_detectionSystem = nullptr;
    BridgeState m_currentState;
    BridgeState m_previousState;
    unsigned long m_stateEntryTime;
    bool beamBreakActive_ = false;

    enum class PendingLowerRequest { NONE, AUTO, MANUAL };
    PendingLowerRequest pendingLowerRequest_ = PendingLowerRequest::NONE;

    void resetBoatCycleState();
    void performSystemReset();
    bool issueLowerBridgeAuto();
    bool issueLowerBridgeManual();
    void processPendingLowerRequest();
};
