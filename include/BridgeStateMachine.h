#pragma once

#include "BridgeSystemDefs.h"
#include "CommandBus.h"
#include "EventBus.h"
#include <Arduino.h>
#include <deque>

class BridgeStateMachine {
public:
    BridgeStateMachine(EventBus& eventBus, CommandBus& commandBus);
    void begin();
    void handleEvent(const BridgeEvent& event);
    void checkTimeouts();  // Check for emergency timeouts
    BridgeState getCurrentState() const;
    String getStateString() const;
    
    static const char* stateName(BridgeState s);

private:
    // Boat passage tracking (tracks left and right boats)
    enum class BoatSide { UNKNOWN, LEFT, RIGHT };
    BoatSide activeBoatSide_ = BoatSide::UNKNOWN;  // Side that first detected the boat
    BoatSide lastEventSide_ = BoatSide::UNKNOWN;   // Side parsed from the most recent boat event
    bool boatCycleActive_ = false;                 // Set to True from first detection until traffic resumes
    
    // Emergency timeout tracking
    unsigned long openingStateEntryTime_ = 0;      // When bridge entered OPENING state

    static const char* sideName(BoatSide s) {
        switch (s) { case BoatSide::LEFT: return "left"; case BoatSide::RIGHT: return "right"; default: return "unknown"; }
    }
    static BoatSide otherSide(BoatSide s) { return s == BoatSide::LEFT ? BoatSide::RIGHT : (s == BoatSide::RIGHT ? BoatSide::LEFT : BoatSide::UNKNOWN); }

    void changeState(BridgeState newState);
    void issueCommand(CommandTarget target, CommandAction action);
    void issueCommand(CommandTarget target, CommandAction action, const String& data);
    void subscribeToEvents();
    
    void onEventReceived(EventData* eventData);
    void handleBoatDetection(BoatSide side);
    bool hasPendingBoatRequests() const { return !boatQueue_.empty(); }
    bool canStartNewCycle() const;
    bool cooldownElapsed() const;
    bool maybeStartPendingCycle();
    void startCooldown();
    void resetCooldown();
    void beginCycleForSide(BoatSide side);
    void startActiveBoatWindow(BoatSide side);
    void endActiveBoatWindow(const char* reason);
    static String boatSideToString(BoatSide side);

    EventBus& m_eventBus;
    CommandBus& m_commandBus;
    BridgeState m_currentState;
    BridgeState m_previousState;
    unsigned long m_stateEntryTime;
    std::deque<BoatSide> boatQueue_;
    bool greenWindowActive_ = false;
    bool cooldownActive_ = false;
    unsigned long cooldownStartTime_ = 0;
    uint8_t sidesServedThisOpening_ = 0;
    bool boatPassedInWindow_ = false;
    static constexpr uint8_t MAX_SIDES_PER_OPEN = 2;

    void resetBoatCycleState(bool clearQueue);
    void performSystemReset();
};
