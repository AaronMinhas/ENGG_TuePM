#include "DetectionSystem.h"

// Constructor initializing the EventBus reference
DetectionSystem::DetectionSystem(EventBus& eventBus)
    : m_eventBus(eventBus), boatDetected(false) {}

// Initialization method
void DetectionSystem::begin() {
    // Initialize any hardware or internal states
    boatDetected = false;  // Ensure boatDetected is reset to false when system begins
}

// Periodic update method
void DetectionSystem::update() {
    detectBoat();
}

// Detecting the boat logic
void DetectionSystem::detectBoat() {
    if (readBoatSensor()) {
        boatDetected = true;
    }
}

// Reading from the boat sensor (just a placeholder function)
bool DetectionSystem::readBoatSensor() {
    // Placeholder logic for sensor reading
    return true; 
}

// New method to check if the system has been initialized
bool DetectionSystem::isInitialized() const {
    return boatDetected;  // Assuming the system is considered initialized if a boat is detected
}
