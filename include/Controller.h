#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"
#include "CommandBus.h"
#include "MotorControl.h"
#include "SignalControl.h"
#include "LocalStateIndicator.h"

class Controller {
public:
    Controller(EventBus& eventBus, CommandBus& commandBus, MotorControl& motorControl, 
               SignalControl& signalControl, LocalStateIndicator& localStateIndicator);
    void begin();
    void handleCommand(const Command& command);

private:
    void subscribeToCommands();
    
    EventBus& m_eventBus;
    CommandBus& m_commandBus;
    MotorControl& m_motorControl;
    SignalControl& m_signalControl;
    LocalStateIndicator& m_localStateIndicator;
};