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
