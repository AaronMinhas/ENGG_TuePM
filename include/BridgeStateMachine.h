#pragma once

#include "BridgeSystemDefs.h"
#include "CommandBus.h"
#include "EventBus.h"
#include <Arduino.h>

class BridgeStateMachine {
public:
    BridgeStateMachine(EventBus& eventBus, CommandBus& commandBus);
    void begin();
    void handleEvent(const BridgeEvent& event);
    BridgeState getCurrentState() const;
    String getStateString() const;

private:
    // Boat passage tracking (tracks left and right boats)
    enum class BoatSide { UNKNOWN, LEFT, RIGHT };
    BoatSide activeBoatSide_ = BoatSide::UNKNOWN;  // Side that first detected the boat
    BoatSide lastEventSide_ = BoatSide::UNKNOWN;   // Side parsed from the most recent boat event
    bool boatCycleActive_ = false;                 // Set to True from first detection until traffic resumes

    static const char* sideName(BoatSide s) {
        switch (s) { case BoatSide::LEFT: return "left"; case BoatSide::RIGHT: return "right"; default: return "unknown"; }
    }
    static BoatSide otherSide(BoatSide s) { return s == BoatSide::LEFT ? BoatSide::RIGHT : (s == BoatSide::RIGHT ? BoatSide::LEFT : BoatSide::UNKNOWN); }

    // Easily readable state names
    static const char* stateName(BridgeState s);

    void changeState(BridgeState newState);
    void issueCommand(CommandTarget target, CommandAction action);
    void issueCommand(CommandTarget target, CommandAction action, const String& data);
    void subscribeToEvents();
    
    void onEventReceived(EventData* eventData);

    EventBus& m_eventBus;
    CommandBus& m_commandBus;
    BridgeState m_currentState;
    BridgeState m_previousState;
    unsigned long m_stateEntryTime;
};
