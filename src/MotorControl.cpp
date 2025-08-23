#include "MotorControl.h"
#include <Arduino.h>

// simulate raise bridge

MotorControl::MotorControl(EventBus& eventBus)
    : m_eventBus(eventBus) {
    }

void MotorControl::raiseBridge() {
    Serial.println("MOTOR CONTROL: Command received -> raiseBridge()");
    Serial.println("MOTOR CONTROL: Raising bridge");

    delay(2000); // fake actuation timer

   Serial.println("MOTOR CONTROL: Action complete. Publish success event.");

    // publish success event
    m_eventBus.publish(BridgeEvent::BRIDGE_OPENED_SUCCESS);
}

void MotorControl::lowerBridge() {
    Serial.println("MOTOR CONTROL: Command received -> lowerBridge()");
    Serial.println("MOTOR CONTROL: Lowering bridge");

    delay(2000); // fake actuation timer

   Serial.println("MOTOR CONTROL: Action complete. Publish success event.");

    // publish success event
    m_eventBus.publish(BridgeEvent::BRIDGE_CLOSED_SUCCESS);
}

void MotorControl::halt() {
  Serial.println("MOTOR CONTROL: Emergency halt command received.");
}