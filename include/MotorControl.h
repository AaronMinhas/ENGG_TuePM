#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"

class MotorControl {
public:
    MotorControl(EventBus& eventBus);
    
    void init();           // Initialise pins and encoder
    void raiseBridge();
    void lowerBridge();
    void halt();
    void checkSerialCommands(); // Check for test commands via serial
    void checkProgress();       // Check motor operation progress (non-blocking)
    
    // Testing and calibration methods
    void testMotor();      // For initial testing
    void testEncoder();    // Test encoder separately
    long getEncoderCount(); // Get current encoder position
    void resetEncoder();   // Reset encoder count to 0

private:
    EventBus& m_eventBus;
    
    // Motor control pins - might need to be changed depending on how we wire stuff up
    static const int MOTOR_PWM_PIN = 23;     // PWM pin for motor speed control
    static const int MOTOR_DIR_PIN_1 = 14;   // Direction control pin 1 (IN1)
    static const int MOTOR_DIR_PIN_2 = 27;   // Direction control pin 2 (IN2)
    
    // Encoder pins -  might need to be changed depending on how we wire stuff up
    static const int ENCODER_PIN_A = 13;      // Interrupt pin for encoder A (must be interrupt pin)
    static const int ENCODER_PIN_B = 12;      // Digital pin for encoder B
    
    // Motor parameters from online documentation
    static const int ENCODER_CPR = 700;      // Counts per revolution (gearbox output)
    static const int MAX_PWM = 255;          // Maximum PWM value
    static const int BRIDGE_TRAVEL_COUNTS = 3500; // Adjust based on your bridge travel distance
    
    // Time-based operation parameters
    static const unsigned long BRIDGE_OPEN_TIME = 8000;  // 8 seconds to open bridge
    static const unsigned long BRIDGE_CLOSE_TIME = 8000; // 8 seconds to close bridge
    
    // Motor control variables
    volatile long m_encoderCount;
    volatile bool m_encoderLastA;
    bool m_motorRunning;
    bool m_raisingBridge;
    bool m_simulationMode;           // Toggle for simulation/test mode
    
    // Non-blocking operation tracking
    unsigned long m_operationStartTime;
    unsigned long m_operationDuration;      // How long this operation should run
    unsigned long m_lastProgressTime;
    static const unsigned long OPERATION_TIMEOUT = 15000; // 15 second timeout
    
    // Private methods
    void setMotorSpeed(int speed, bool forward);
    void stopMotor();
    bool isTargetReached(long targetCount);
    
    // Static interrupt handler
    static MotorControl* s_instance;
    static void IRAM_ATTR encoderISR();
};