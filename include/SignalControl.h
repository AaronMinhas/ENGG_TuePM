#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class SignalControl {
public:
    SignalControl(EventBus& eventBus);
    
    void stopTraffic();
    void resumeTraffic();
    void halt();
    
    // Individual light control methods
    void setCarLight(const String& side, const String& color);
    void setBoatLight(const String& side, const String& color);
    
private:
    EventBus& m_eventBus;
};
