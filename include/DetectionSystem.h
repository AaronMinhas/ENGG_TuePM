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
    void setSimulationUltrasonicEnabled(bool leftEnabled, bool rightEnabled);
    void setSimulationBeamBreakEnabled(bool enabled);
    void setSimulationUltrasonicLeftEnabled(bool enabled);
    void setSimulationUltrasonicRightEnabled(bool enabled);

    struct SimulationSensorConfig {
        bool ultrasonicLeftEnabled;
        bool ultrasonicRightEnabled;
        bool beamBreakEnabled;
    };

    SimulationSensorConfig getSimulationSensorConfig() const;

    // Direction information
    enum class BoatDirection {
        NONE,
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT
    };
    
    // Beam break sensor methods
    bool readBeamBreak() const;  // Returns true if beam is broken (boat present)

    // Sensor control (called by state machine)
    void enableAllSensors();  // Re-enable both sensors after bridge cycle completes
    void enableOppositeSensor(BoatDirection direction);  // Re-enable opposite sensor during clearance period
    void resetBoatDetectionState();  // Reset boat detection state to allow opposite sensor to detect

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
    bool m_simUltrasonicLeftEnabled = false;
    bool m_simUltrasonicRightEnabled = false;
    bool m_simBeamBreakEnabled = false;
    // Boat detection state (tracks the active direction - no queuing)
    bool boatDetected = false;
    BoatDirection boatDirection = BoatDirection::NONE;
    
    // Sensor enable/disable control
    bool leftSensorEnabled = true;
    bool rightSensorEnabled = true;
    
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
    void publishSimulationSensorConfig() const;
    bool allowUltrasonicEvents(bool leftSensor) const;
    bool allowBeamBreakEvents() const;
    void disableOppositeSensor(BoatDirection direction);
};
