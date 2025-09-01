#include "MotorControl.h"
#include <Arduino.h>
#include "EventBus.h"

MotorControl::MotorControl(EventBus& eventBus)
    : m_eventBus(eventBus) {
}

void MotorControl::raiseBridge() {
    Serial.println("MOTOR CONTROL: Command received -> raiseBridge()");
    Serial.println("MOTOR CONTROL: Raising bridge");

    // delay(2000);  ← temp removed to debug esp32 crash

    Serial.println("MOTOR CONTROL: Action complete. Publishing success event.");

    // Publish immediately since there's no blocking delay
    auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_OPENED_SUCCESS);
    m_eventBus.publish(BridgeEvent::BRIDGE_OPENED_SUCCESS, eventData);
    
    Serial.println("MOTOR CONTROL: Success event published");
}

void MotorControl::lowerBridge() {
    Serial.println("MOTOR CONTROL: Command received -> lowerBridge()");
    Serial.println("MOTOR CONTROL: Lowering bridge");

    // delay(2000);  ← temp removed to debug esp32 crash

    Serial.println("MOTOR CONTROL: Action complete. Publishing success event.");

    auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_CLOSED_SUCCESS);
    m_eventBus.publish(BridgeEvent::BRIDGE_CLOSED_SUCCESS, eventData);
    
    Serial.println("MOTOR CONTROL: Success event published");
}

void MotorControl::halt() {
    Serial.println("MOTOR CONTROL: Emergency halt command received.");
}