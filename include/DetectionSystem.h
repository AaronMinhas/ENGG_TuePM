#pragma once

#ifdef UNIT_TEST
// For unit testing, Arduino header is not needed.
#else
    #include <Arduino.h>  // Arduino specific header if not testing
#endif

#include "EventBus.h"  // Include EventBus header

class DetectionSystem {
public:
    DetectionSystem(EventBus& eventBus);  // Constructor accepting EventBus reference

    void begin();   // Initialization method
    void update();  // Method to be called periodically

    // New method to check if the system has been initialized
    bool isInitialized() const;  // Add this method to check initialization status

private:
    EventBus& m_eventBus;  // Reference to EventBus instance
    bool boatDetected;     // Flag to store detection status

    void detectBoat();        // Function to check if a boat is detected
    bool readBoatSensor();    // Function to read from boat sensor
};
