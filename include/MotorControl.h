#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class MotorControl {
public:
    MotorControl(EventBus& eventBus);

void raiseBridge();

void lowerBridge();

void halt();

private:
    EventBus& m_eventBus;

};