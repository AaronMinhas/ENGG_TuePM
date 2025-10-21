#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"
#include "CommandBus.h"
#include <Arduino.h>
#include <mutex>

/*
 * SafetyManager
 * 
 * Purpose:
 * - Monitor system safety conditions and respond to safety-critical events
 * - Enforce state transition timeouts
 * - Force the system into safe state when faults are detected
 * - Directly control hardware in emergency situations
 * 
 * Integration:
 * - Subscribes to all events for safety monitoring
 * - Monitors state transition times
 * - Directly calls motor stop and light control functions in emergencies
 */

class SafetyManager {
public:
    // Constructor with required dependencies
    SafetyManager(EventBus& eventBus, CommandBus& commandBus);
    
    // Initialize the safety manager
    void begin();
    
    // Update method called from main loop to handle periodic safety checks
    void update();
    
    // Emergency stop handling
    void triggerEmergency(const char* reason);
    bool isEmergencyActive() const;
    void clearEmergency();
    
    // Direct control references for immediate actions during emergencies
    // These must be set during initialization
    void setMotorControl(class MotorControl* motorControl);
    void setSignalControl(class SignalControl* signalControl);
    
    // Simulation mode for testing
    void setSimulationMode(bool enabled);
    bool isSimulationMode() const;

    bool testFaultActive = false;
    void triggerTestFault();        // Trigger a manual test fault
    void clearTestFault();          // Clear test fault state
    bool isTestFaultActive() const; // Query test fault state

private:
    EventBus& m_eventBus;
    CommandBus& m_commandBus;
    
    // Direct references for emergency control
    class MotorControl* m_motorControl;
    class SignalControl* m_signalControl;
    
    // Thread safety
    std::mutex m_mutex;
    
    // Safety state
    bool m_emergencyActive;
    bool m_simulationMode;
    
    // State transition monitoring
    BridgeEvent m_lastStateEvent;
    unsigned long m_stateEventTime;
    
    // Expected state transitions and timeouts (milliseconds)
    static constexpr unsigned long BOAT_DETECTED_TIMEOUT_MS = 2000;        // 2 seconds
    static constexpr unsigned long TRAFFIC_STOPPED_TIMEOUT_MS = 8000;      // 8 seconds
    static constexpr unsigned long BRIDGE_OPENED_TIMEOUT_MS = 10000;       // 10 seconds
    static constexpr unsigned long BOAT_PASSED_TIMEOUT_MS = 2000;          // 2 seconds
    static constexpr unsigned long BRIDGE_CLOSED_TIMEOUT_MS = 2000;        // 2 seconds
    
    // Last fault reason for logging
    String m_lastFaultReason;
    
    // Private methods
    void enterSafeState(const char* reason);
    void checkStateTransitionTimeouts();
    void onEvent(EventData* data);
    void handleCommand(const Command& command);
    
    // Get expected next state for a given state event
    BridgeEvent getExpectedNextEvent(BridgeEvent currentEvent);
    
    // Get timeout for a given state event
    unsigned long getStateTimeout(BridgeEvent stateEvent);
};