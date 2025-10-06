#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class SignalControl {
public:
    SignalControl(EventBus& eventBus);
    
    void begin();
    void stopTraffic();
    void resumeTraffic();
    void halt();
    
    // Car traffic controls (both sides together)
    void setCarTraffic(const String& color);
    void setBoatLight(const String& side, const String& color);
    
private:
    EventBus& m_eventBus;
};
