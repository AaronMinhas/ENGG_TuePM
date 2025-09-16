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

    // Simulation mode controls (disables event publishing but still measures distance)
    void setSimulationMode(bool enable);
    bool isSimulationMode() const;

    // Debug/status helpers
    float getFilteredDistanceCm() const;  // Returns EMA-filtered distance (cm), <0 if unknown
    int getZoneIndex() const;             // 0=far,1=near,2=close,3=none
    const char* zoneName() const;         // Human-readable

private:
    EventBus& m_eventBus;  // Reference to EventBus instance
    bool boatDetected;     // Latched detection state (critical proximity debounced)
    bool m_simulationMode = false; // When true, suppress event publishing

    // Ultrasonic sensing
    void detectBoat();           // Main detection routine (zones + debounce)
    float readDistanceCm();      // Measure distance in centimeters (returns <0 on timeout)
    bool inCriticalRange(float cm) const; // Threshold check for eventing

    // Internal timing/filtering
    unsigned long lastSampleMs = 0;
    float emaDistanceCm = -1.0f;   // Exponential moving average distance
    int lastZone = -1;             // -1 unknown, 0 far, 1 near, 2 close, 3 none
    unsigned long criticalEnterMs = 0;
    unsigned long clearEnterMs = 0;
};
