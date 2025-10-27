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
    void resetToIdleState();
    void update();  // Called from main loop to process timing
    
    // Car traffic controls (both sides together)
    void setCarTraffic(const String& color);
    void setBoatLight(const String& side, const String& color);
    
    // Boat queue timer methods
    void startBoatGreenPeriod(const String& side);  // Start 45s green period for boat queue
    void endBoatGreenPeriod();                      // End green period, turn lights red
    bool isBoatGreenPeriodActive() const;           // Check if queue timer is active
    
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
    
    // Boat queue timer state
    bool m_boatQueueActive;
    unsigned long m_boatQueueStartTime;
    String m_boatQueueSide;  // Which side's light is green ("left" or "right")
};
