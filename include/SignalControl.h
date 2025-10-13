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
    void update();  // Called from main loop to process timing
    
    // Car traffic controls (both sides together)
    void setCarTraffic(const String& color);
    void setBoatLight(const String& side, const String& color);
    
private:
    EventBus& m_eventBus;
    
    // Timing state for non-blocking operations
    enum class Operation {
        NONE,
        STOPPING_TRAFFIC,
        RESUMING_TRAFFIC
    };
    
    enum class StopPhase {
        YELLOW_WARNING,    // Green→Yellow
        RED_CLEARANCE,     // Yellow→Red
        COMPLETE           // Publish SUCCESS
    };
    
    enum class ResumePhase {
        RED_WAITING,       // Stay Red
        GREEN_GO,          // Red→Green, publish SUCCESS
        COMPLETE
    };
    
    Operation m_currentOperation;
    StopPhase m_stopPhase;
    ResumePhase m_resumePhase;
    unsigned long m_operationStartTime;
};
