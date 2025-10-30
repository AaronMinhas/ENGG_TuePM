#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class MotorControl {
public:
    MotorControl(EventBus& eventBus);
    
    void init();           // Initialise pins
    void raiseBridge();
    void lowerBridge();
    void halt();
    void checkProgress();       // Check motor operation progress (non-blocking)
    
    // Testing and calibration methods
    void testMotor();      // For initial testing

    // Simulation mode control (for external CLI router)
    void setSimulationMode(bool enable) { m_simulationMode = enable; }
    bool isSimulationMode() const { return m_simulationMode; }

    // Limit switch diagnostics
    int getLimitSwitchRaw() const { return digitalRead(LIMIT_SWITCH_PIN); }
    bool isLimitSwitchActive() const { return getLimitSwitchRaw() == LIMIT_SWITCH_ACTIVE_STATE; }

private:
    EventBus& m_eventBus;
    
    // Motor control pins - might need to be changed depending on how we wire stuff up
    static const int MOTOR_PWM_PIN = 23;     // PWM pin for motor speed control
    static const int MOTOR_DIR_PIN_1 = 14;   // Direction control pin 1 (IN1)
    static const int MOTOR_DIR_PIN_2 = 27;   // Direction control pin 2 (IN2)
    
    // Limit switch pin (shared by both end-stop buttons)
    static const int LIMIT_SWITCH_PIN = 13;   // GPIO pin for limit switches
    static const int LIMIT_SWITCH_ACTIVE_STATE = LOW;
    static const unsigned long LIMIT_RELEASE_GRACE_MS = 300; // Ignore limit re-trigger briefly after leaving switch

    static const int MAX_PWM = 255;          // Maximum PWM value
    
    // Motor control variables
    bool m_motorRunning;
    bool m_raisingBridge;
    bool m_simulationMode;           // Toggle for simulation/test mode
    bool m_limitCleared;
    
    // Grace handling
    bool m_inGracePeriod;
    unsigned long m_graceEndsAt;
    
    // Private methods
    void setMotorSpeed(int speed, bool forward);
    void stopMotor();
};
