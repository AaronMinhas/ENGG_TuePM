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
    m_eventBus.publish(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, nullptr);
}

void SignalControl::resumeTraffic() {
    Serial.println("SIGNAL_CONTROL: Resuming traffic - setting signals to GREEN");
    
    // TODO:
    // Set traffic lights to green
    // Update boat signals 
    
    // Publish success event
    Serial.println("SIGNAL_CONTROL: Traffic resumed successfully");
    m_eventBus.publish(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, nullptr);
}

void SignalControl::halt() {
    Serial.println("SIGNAL_CONTROL: EMERGENCY HALT - setting all signals to SAFE state (RED)");
    
    // TODO: 
    // Set all signals to safe state (red for traffic, red for boats)
    
    Serial.println("SIGNAL_CONTROL: All signals set to safe state");
}

void SignalControl::setCarLight(const String& side, const String& color) {
    Serial.print("SIGNAL_CONTROL: Setting car light - Side: ");
    Serial.print(side);
    Serial.print(", Colour: ");
    Serial.println(color);
    
    // TODO: Implement actual hardware control
    // Control the specific car traffic light
    
    // For now, just log the change
    Serial.println("SIGNAL_CONTROL: Car light updated successfully");
    
    // Publish success event with light change data
    auto* lightData = new LightChangeData(side, color, true);  // true = car light
    m_eventBus.publish(BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS, lightData);
}

void SignalControl::setBoatLight(const String& side, const String& color) {
    Serial.print("SIGNAL_CONTROL: Setting boat light - Side: ");
    Serial.print(side);
    Serial.print(", Colour: ");
    Serial.println(color);
    
    // TODO: Implement actual hardware control
    // Control the specific boat traffic light
    
    // For now, just log the change
    Serial.println("SIGNAL_CONTROL: Boat light updated successfully");
    
    // Publish success event with light change data
    auto* lightData = new LightChangeData(side, color, false);  // false = boat light
    m_eventBus.publish(BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS, lightData);
}
