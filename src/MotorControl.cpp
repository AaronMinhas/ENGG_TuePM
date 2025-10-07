#include "MotorControl.h"
#include <Arduino.h>
#include "EventBus.h"
#include "Logger.h"

// Static instance pointer for interrupt handler
MotorControl* MotorControl::s_instance = nullptr;

MotorControl::MotorControl(EventBus& eventBus)
    : m_eventBus(eventBus), 
      m_encoderCount(0),
      m_encoderLastA(false),
      m_motorRunning(false),
      m_raisingBridge(false),
      m_simulationMode(false) {      // Start in real mode by default
    s_instance = this; // Set static instance for interrupt handler
}

void MotorControl::init() {
    LOG_INFO(Logger::TAG_MC, "Initialising motor and encoder...");
    
    // Configure motor control pins
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    pinMode(MOTOR_DIR_PIN_1, OUTPUT);
    pinMode(MOTOR_DIR_PIN_2, OUTPUT);
    
    // Configure encoder pins
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    
    // Attach interrupt for encoder
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, CHANGE);
    
    // Initialise motor to stopped state
    stopMotor();
    
    // Reset encoder count
    resetEncoder();
    
    LOG_INFO(Logger::TAG_MC, "Initialisation complete");
    LOG_INFO(Logger::TAG_MC, "Using pins - PWM: %d, DIR1: %d, DIR2: %d, ENC_A: %d, ENC_B: %d",
             MOTOR_PWM_PIN, MOTOR_DIR_PIN_1, MOTOR_DIR_PIN_2, ENCODER_PIN_A, ENCODER_PIN_B);
}

void MotorControl::raiseBridge() {
    LOG_INFO(Logger::TAG_MC, "Command received -> raiseBridge()");
    
    if (m_motorRunning) {
        LOG_WARN(Logger::TAG_MC, "Motor already running, ignoring command");
        return;
    }
    
    // SIMULATION MODE: Skip actual motor control
    if (m_simulationMode) {
        LOG_INFO(Logger::TAG_MC, "SIMULATION MODE - Simulating bridge raise");
        m_motorRunning = true;
        m_raisingBridge = true;
        
        // Simulate movement with a short delay
        delay(2000); // 2 seconds simulation delay
        
        m_motorRunning = false;
        LOG_INFO(Logger::TAG_MC, "Bridge raised successfully (SIMULATED)");
        
        // Publish success event
        auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_OPENED_SUCCESS);
        m_eventBus.publish(BridgeEvent::BRIDGE_OPENED_SUCCESS, eventData);
        
        LOG_DEBUG(Logger::TAG_MC, "Success event published");
        return;
    }
    
    // REAL MODE: Time-based motor control
    m_raisingBridge = true;
    m_motorRunning = true;
    
    LOG_INFO(Logger::TAG_MC, "Starting to raise bridge (TIME-BASED)");
    LOG_DEBUG(Logger::TAG_MC, "Will run for %lu milliseconds", BRIDGE_OPEN_TIME);
    
    // Store operation parameters for non-blocking operation
    m_operationStartTime = millis();
    m_operationDuration = BRIDGE_OPEN_TIME;
    m_lastProgressTime = millis();
    
    // Start motor in forward direction (adjust speed as needed)
    setMotorSpeed(180, true); // 180/255 = ~70% speed, forward direction
    
    LOG_DEBUG(Logger::TAG_MC, "Non-blocking operation started. Use checkProgress() to monitor.");
}

void MotorControl::lowerBridge() {
    LOG_INFO(Logger::TAG_MC, "Command received -> lowerBridge()");
    
    if (m_motorRunning) {
        LOG_WARN(Logger::TAG_MC, "Motor already running, ignoring command");
        return;
    }
    
    // SIMULATION MODE: Skip actual motor control
    if (m_simulationMode) {
        LOG_INFO(Logger::TAG_MC, "SIMULATION MODE - Simulating bridge lower");
        m_motorRunning = true;
        m_raisingBridge = false;
        
        // Simulate movement with a short delay
        delay(2000); // 2 seconds simulation delay
        
        m_motorRunning = false;
        LOG_INFO(Logger::TAG_MC, "Bridge lowered successfully (SIMULATED)");
        
        // Publish success event
        auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_CLOSED_SUCCESS);
        m_eventBus.publish(BridgeEvent::BRIDGE_CLOSED_SUCCESS, eventData);
        
        LOG_DEBUG(Logger::TAG_MC, "Success event published");
        return;
    }
    
    // REAL MODE: Time-based motor control
    m_raisingBridge = false;
    m_motorRunning = true;
    
    LOG_INFO(Logger::TAG_MC, "Starting to lower bridge (TIME-BASED)");
    LOG_DEBUG(Logger::TAG_MC, "Will run for %lu milliseconds", BRIDGE_CLOSE_TIME);
    
    // Store operation parameters for non-blocking operation
    m_operationStartTime = millis();
    m_operationDuration = BRIDGE_CLOSE_TIME;
    m_lastProgressTime = millis();
    
    // Start motor in reverse direction (adjust speed as needed)
    setMotorSpeed(180, false);
    
    LOG_DEBUG(Logger::TAG_MC, "Non-blocking operation started. Use checkProgress() to monitor.");
}

void MotorControl::checkProgress() {
    if (!m_motorRunning) {
        return; // No operation in progress
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - m_operationStartTime;
    
    // Print progress every 1000ms (1 second)
    if (currentTime - m_lastProgressTime >= 1000) {
        float percentComplete = (float)elapsedTime / (float)m_operationDuration * 100.0;
        LOG_DEBUG(Logger::TAG_MC, "Progress - %.1f%% complete (%lu/%lu ms)",
                  percentComplete, elapsedTime, m_operationDuration);
        m_lastProgressTime = currentTime;
    }
    
    // Check if operation duration completed
    if (elapsedTime >= m_operationDuration) {
        stopMotor();
        LOG_INFO(Logger::TAG_MC, "Bridge operation completed successfully after %lu ms", elapsedTime);
        
        // Publish success event
        BridgeEvent eventType = m_raisingBridge ? BridgeEvent::BRIDGE_OPENED_SUCCESS : BridgeEvent::BRIDGE_CLOSED_SUCCESS;
        auto* eventData = new SimpleEventData(eventType);
        m_eventBus.publish(eventType, eventData);
        
        LOG_DEBUG(Logger::TAG_MC, "Success event published");
        return;
    }
    
    // Check for timeout (safety fallback)
    if (elapsedTime >= OPERATION_TIMEOUT) {
        stopMotor();
        LOG_ERROR(Logger::TAG_MC, "Bridge operation timed out (safety timeout)!");
        LOG_ERROR(Logger::TAG_MC, "Timeout details - Elapsed: %lu ms, Duration: %lu ms",
                  elapsedTime, m_operationDuration);
        return;
    }
}

void MotorControl::halt() {
    LOG_WARN(Logger::TAG_MC, "Emergency halt command received.");
    stopMotor();
    LOG_WARN(Logger::TAG_MC, "Motor stopped immediately");
}

void MotorControl::testMotor() {
    LOG_INFO(Logger::TAG_MC, "Starting motor test sequence...");
    
    // Test forward direction
    LOG_INFO(Logger::TAG_MC, "Testing forward direction for 2 seconds");
    setMotorSpeed(100, true); // Low speed forward
    delay(2000);
    stopMotor();
    delay(1000);
    
    // Test reverse direction
    LOG_INFO(Logger::TAG_MC, "Testing reverse direction for 2 seconds");
    setMotorSpeed(100, false); // Low speed reverse
    delay(2000);
    stopMotor();
    
    LOG_INFO(Logger::TAG_MC, "Test complete. Final encoder count: %ld", m_encoderCount);
}

void MotorControl::testEncoder() {
    LOG_INFO(Logger::TAG_MC, "Testing encoder...");
    LOG_INFO(Logger::TAG_MC, "Manually rotate the motor shaft and watch the count change");
    LOG_INFO(Logger::TAG_MC, "Press any key to stop the test");
    
    long lastCount = m_encoderCount;
    unsigned long lastPrint = millis();
    
    while (!Serial.available()) {
        if (millis() - lastPrint > 200) { // Print every 200ms
            if (m_encoderCount != lastCount) {
                LOG_INFO(Logger::TAG_MC, "Encoder count: %ld (changed by %ld)",
                         m_encoderCount, m_encoderCount - lastCount);
                lastCount = m_encoderCount;
            }
            lastPrint = millis();
        }
        delay(10);
    }
    
    // Clear serial buffer
    while (Serial.available()) {
        Serial.read();
    }
    
    LOG_INFO(Logger::TAG_MC, "Encoder test complete. Final count: %ld", m_encoderCount);
}

 

long MotorControl::getEncoderCount() {
    return m_encoderCount;
}

void MotorControl::resetEncoder() {
    m_encoderCount = 0;
    LOG_INFO(Logger::TAG_MC, "Encoder count reset to 0");
}

// Private methods

void MotorControl::setMotorSpeed(int speed, bool forward) {
    // Constrain speed to valid PWM range
    speed = constrain(speed, 0, MAX_PWM);
    
    if (forward) {
        // Forward direction: DIR1 = HIGH, DIR2 = LOW
        digitalWrite(MOTOR_DIR_PIN_1, HIGH);
        digitalWrite(MOTOR_DIR_PIN_2, LOW);
    } else {
        // Reverse direction: DIR1 = LOW, DIR2 = HIGH
        digitalWrite(MOTOR_DIR_PIN_1, LOW);
        digitalWrite(MOTOR_DIR_PIN_2, HIGH);
    }
    
    // Set PWM speed
    analogWrite(MOTOR_PWM_PIN, speed);
    
    LOG_DEBUG(Logger::TAG_MC, "Motor set to speed %d, direction %s",
              speed, forward ? "FORWARD" : "REVERSE");
}

void MotorControl::stopMotor() {
    // Stop motor by setting both direction pins LOW and PWM to 0
    digitalWrite(MOTOR_DIR_PIN_1, LOW);
    digitalWrite(MOTOR_DIR_PIN_2, LOW);
    analogWrite(MOTOR_PWM_PIN, 0);
    
    m_motorRunning = false;
    
    LOG_INFO(Logger::TAG_MC, "Motor stopped");
}

bool MotorControl::isTargetReached(long targetCount) {
    // Allow some tolerance for target position (±5 counts)
    const int tolerance = 5;
    
    if (m_raisingBridge) {
        return m_encoderCount >= (targetCount - tolerance);
    } else {
        return m_encoderCount <= (targetCount + tolerance);
    }
}

// Static interrupt service routine for encoder
void IRAM_ATTR MotorControl::encoderISR() {
    if (s_instance) {
        // Read current state of encoder A
        bool currentA = digitalRead(s_instance->ENCODER_PIN_A);
        
        // Check for rising edge on encoder A
        if (!s_instance->m_encoderLastA && currentA) {
            // Read encoder B to determine direction
            bool encoderB = digitalRead(s_instance->ENCODER_PIN_B);
            
            if (encoderB) {
                s_instance->m_encoderCount++; // Forward/CW
            } else {
                s_instance->m_encoderCount--; // Reverse/CCW
            }
        }
        
        s_instance->m_encoderLastA = currentA;
    }
}
