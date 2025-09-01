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
    
    // Testing and calibration methods
    void testMotor();      // For initial testing
    void testEncoder();    // Test encoder separately
    long getEncoderCount(); // Get current encoder position
    void resetEncoder();   // Reset encoder count to 0

private:
    EventBus& m_eventBus;
    
    // Motor control pins - might need to be changed depending on how we wire stuff up
    static const int MOTOR_PWM_PIN = 13;     // PWM pin for motor speed control
    static const int MOTOR_DIR_PIN_1 = 12;   // Direction control pin 1 (IN1)
    static const int MOTOR_DIR_PIN_2 = 14;   // Direction control pin 2 (IN2)
    
    // Encoder pins -  might need to be changed depending on how we wire stuff up
    static const int ENCODER_PIN_A = 2;      // Interrupt pin for encoder A (must be interrupt pin)
    static const int ENCODER_PIN_B = 4;      // Digital pin for encoder B
    
    // Motor parameters from online documentation
    static const int ENCODER_CPR = 700;      // Counts per revolution (gearbox output)
    static const int MAX_PWM = 255;          // Maximum PWM value
    static const int BRIDGE_TRAVEL_COUNTS = 3500; // Adjust based on your bridge travel distance
    
    // Motor control variables
    volatile long m_encoderCount;
    volatile bool m_encoderLastA;
    bool m_motorRunning;
    bool m_raisingBridge;
    bool m_simulationMode;           // Toggle for simulation/test mode
    
    // Private methods
    void setMotorSpeed(int speed, bool forward);
    void stopMotor();
    bool isTargetReached(long targetCount);
    
    // Static interrupt handler
    static MotorControl* s_instance;
    static void IRAM_ATTR encoderISR();
};