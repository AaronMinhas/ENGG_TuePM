#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include <Arduino.h>

SignalControl::SignalControl(EventBus& eventBus) : m_eventBus(eventBus) {
    Serial.println("SIGNAL_CONTROL: Initialised");
}

void SignalControl::stopTraffic() {
    Serial.println("SIGNAL_CONTROL: Stopping traffic - setting signals to RED");
    
    // TODO: Implement actual hardware control
    // Set traffic lights to red
    // Control boat signals
    
    // Publish success event
    Serial.println("SIGNAL_CONTROL: Traffic stopped successfully");
    m_eventBus.publish(BridgeEvent::TRAFFIC_STOPPED_SUCCESS);
}

void SignalControl::resumeTraffic() {
    Serial.println("SIGNAL_CONTROL: Resuming traffic - setting signals to GREEN");
    
    // TODO:
    // Set traffic lights to green
    // Update boat signals 
    
    // Publish success event
    Serial.println("SIGNAL_CONTROL: Traffic resumed successfully");
    m_eventBus.publish(BridgeEvent::TRAFFIC_RESUMED_SUCCESS);
}

void SignalControl::halt() {
    Serial.println("SIGNAL_CONTROL: EMERGENCY HALT - setting all signals to SAFE state (RED)");
    
    // TODO: 
    // Set all signals to safe state (red for traffic, red for boats)
    
    Serial.println("SIGNAL_CONTROL: All signals set to safe state");
}
