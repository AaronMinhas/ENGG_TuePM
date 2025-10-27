#pragma once

#include <Arduino.h>
#include <deque>
#include "EventBus.h"

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

    // Direction information
    enum class BoatDirection {
        NONE,
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT
    };
    
    // Beam break sensor methods
    bool readBeamBreak() const;  // Returns true if beam is broken (boat present)

    // Debug/status helpers
    float getLeftFilteredDistanceCm() const;  // Returns EMA-filtered distance (cm), <0 if unknown
    float getRightFilteredDistanceCm() const;
    int getLeftZoneIndex() const;             // 0=far,1=near,2=close,3=none
    int getRightZoneIndex() const;
    const char* getLeftZoneName() const;         // Human-readable
    const char* getRightZoneName() const;
    BoatDirection getCurrentDirection() const;
    const char* getDirectionName() const;

private:
    EventBus& m_eventBus;  // Reference to EventBus instance
    bool m_simulationMode = false; // When true, suppress event publishing
    // Boat detection state (tracks the active direction plus queued boats waiting to cross)
    bool boatDetected = false;
    BoatDirection boatDirection = BoatDirection::NONE;
    
    // Left sensor variables
    float leftEmaDistanceCm = -1.0f;
    int leftLastZone = -1;
    int leftPrevZone = 3;
    unsigned long leftCriticalEnterMs = 0;
    bool leftApproachActive = false;
    
    // Right sensor variables
    float rightEmaDistanceCm = -1.0f;
    int rightLastZone = -1;
    int rightPrevZone = 3;
    unsigned long rightCriticalEnterMs = 0;
    bool rightApproachActive = false;
    
    // Beam break sensor tracking (for debouncing)
    bool beamBroken = false;
    unsigned long beamBrokenEnterMs = 0;
    unsigned long beamClearEnterMs = 0;

    // Pending boats detected while another boat is being processed
    std::deque<BoatDirection> pendingBoatDirections;
    BoatDirection pendingPriorityDirection = BoatDirection::NONE;
    
    // Timing
    unsigned long lastSampleMs = 0;
    
    // Ultrasonic sensing methods
    float readDistanceCm(int trigPin, int echoPin);
    bool inCriticalRange(float cm) const;
    int getZoneFromDistance(float distance) const;
    void updateFilteredDistances(float leftRawDist, float rightRawDist);
    void updateZones();
    
    // Detection methods
    void checkInitialDetection();
    void checkBoatPassed();
};
