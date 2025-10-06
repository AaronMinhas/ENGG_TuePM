#ifdef UNIT_TEST
// Mock millis() for unit testing
extern unsigned long mock_millis;
unsigned long millis() {
    return mock_millis;
}
#else
#include <Arduino.h>
#define BOAT_SENSOR_PIN 5  // This Pin number can be changed
#endif

#include "DetectionSystem.h"

// Constructor initializing the EventBus reference
DetectionSystem::DetectionSystem(EventBus& eventBus)
    : m_eventBus(eventBus), boatDetected(false) {}

// Initialization method
void DetectionSystem::begin() {
    // Initialize any hardware or internal states
    #ifndef UNIT_TEST
    pinMode(BOAT_SENSOR_PIN, INPUT);
    #endif
    boatDetected = false;  // Ensure boatDetected is reset to false when system begins
}

// Periodic update method
void DetectionSystem::update() {
    detectBoat();
}

// Detecting the boat logic
void DetectionSystem::detectBoat() {
    // Debounce logic: require sensor to be true for a minimum duration
    static unsigned long lastDetectionTime = 0;
    static bool lastSensorState = false;
    const unsigned long debounceDelay = 1000; // milliseconds

    // Read the sensor state from the hardware pin
    // bool currentSensorState = digitalRead(BOAT_SENSOR_PIN); // this is real
    bool currentSensorState = readBoatSensor(); // This is for mock
    unsigned long now = millis();

    if (currentSensorState) {
        if (!lastSensorState) {
            // Sensor just went HIGH, start debounce timer
            lastDetectionTime = now;
        } else if ((now - lastDetectionTime) >= debounceDelay) {
            // Sensor has been HIGH for debounceDelay, confirm detection
            if (!boatDetected) {
                boatDetected = true;
                // Publish BOAT_DETECTED event to EventBus
                m_eventBus.publish(BridgeEvent::BOAT_DETECTED, nullptr, EventPriority::NORMAL);
            }
        }
    } else {
        if (boatDetected) {
            // Sensor went LOW, boat has passed
            boatDetected = false;
            // Publish BOAT_PASSED event to EventBus
            m_eventBus.publish(BridgeEvent::BOAT_PASSED, nullptr, EventPriority::NORMAL);
        }
        lastDetectionTime = now;
    }
    lastSensorState = currentSensorState;
}

// Reading from the boat sensor (just a placeholder function)
bool DetectionSystem::readBoatSensor() {
    // Directly read the sensor pin
   // return digitalRead(BOAT_SENSOR_PIN); // This is real
   return true; 
}

// New method to check if the system has been initialized
bool DetectionSystem::isInitialized() const {
    return boatDetected;  // Assuming the system is considered initialized if a boat is detected
}
