#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"
#include "MotorControl.h"
#include "SignalControl.h"
#include "LocalStateIndicator.h"

class Controller {
public:
    Controller(EventBus& eventBus, MotorControl& motorControl);
    void handleCommand(const Command& command);

private:
    EventBus& m_eventBus;
    MotorControl& m_motorControl;
    //TODO: other subsystems
};