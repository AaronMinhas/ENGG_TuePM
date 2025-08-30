#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class SignalControl {
public:
    SignalControl(EventBus& eventBus);
    
    void stopTraffic();
    void resumeTraffic();
    void halt();
    
private:
    EventBus& m_eventBus;
};
