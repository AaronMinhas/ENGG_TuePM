#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class LocalStateIndicator {
public:
    LocalStateIndicator(EventBus& eventBus);
    
    void setState();
    void halt();
    
private:
    EventBus& m_eventBus;
};
