#include "MotorControl.h"
#include <Arduino.h>
#include "EventBus.h"

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
    Serial.println("MOTOR CONTROL: Initializing motor and encoder...");
    
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
    
    Serial.println("MOTOR CONTROL: Initialization complete");
    Serial.printf("MOTOR CONTROL: Using pins - PWM: %d, DIR1: %d, DIR2: %d, ENC_A: %d, ENC_B: %d\n", 
                  MOTOR_PWM_PIN, MOTOR_DIR_PIN_1, MOTOR_DIR_PIN_2, ENCODER_PIN_A, ENCODER_PIN_B);
}

void MotorControl::raiseBridge() {
    Serial.println("MOTOR CONTROL: Command received -> raiseBridge()");
    
    if (m_motorRunning) {
        Serial.println("MOTOR CONTROL: Motor already running, ignoring command");
        return;
    }
    
    // SIMULATION MODE: Skip actual motor control
    if (m_simulationMode) {
        Serial.println("MOTOR CONTROL: SIMULATION MODE - Simulating bridge raise");
        m_motorRunning = true;
        m_raisingBridge = true;
        
        // Simulate movement with a short delay
        delay(2000); // 2 seconds simulation delay
        
        m_motorRunning = false;
        Serial.println("MOTOR CONTROL: Bridge raised successfully (SIMULATED)");
        
        // Publish success event
        auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_OPENED_SUCCESS);
        m_eventBus.publish(BridgeEvent::BRIDGE_OPENED_SUCCESS, eventData);
        
        Serial.println("MOTOR CONTROL: Success event published");
        return;
    }
    
    // REAL MODE: Actual motor control
    m_raisingBridge = true;
    m_motorRunning = true;
    
    Serial.println("MOTOR CONTROL: Starting to raise bridge");
    Serial.printf("MOTOR CONTROL: Current encoder count: %ld\n", m_encoderCount);
    
    // ENCODER-BASED CONTROL: Use encoder feedback for precise positioning
    long startCount = m_encoderCount;
    long targetCount = startCount + BRIDGE_TRAVEL_COUNTS; // Move by bridge travel distance
    
    Serial.printf("MOTOR CONTROL: Target count: %ld (travel: %d counts)\n", targetCount, BRIDGE_TRAVEL_COUNTS);
    
    // Start motor in forward direction (adjust speed as needed)
    setMotorSpeed(180, true); // 180/255 = ~70% speed, forward direction
    
    // Monitor movement until target is reached
    unsigned long startTime = millis();
    unsigned long timeout = 15000; // 15 second timeout for safety
    
    while (m_motorRunning && !isTargetReached(targetCount) && (millis() - startTime) < timeout) {
        delay(10); // Small delay to prevent overwhelming the serial output
        
        // Print progress every 500ms
        if ((millis() - startTime) % 500 == 0) {
            Serial.printf("MOTOR CONTROL: Progress - Count: %ld, Target: %ld\n", m_encoderCount, targetCount);
        }
    }
    
    // Stop motor
    stopMotor();
    
    if (millis() - startTime >= timeout) {
        Serial.println("MOTOR CONTROL: Bridge raise operation timed out!");
        // TODO: In real implementation, trigger fault event
        return;
    }
    
    Serial.printf("MOTOR CONTROL: Bridge raised successfully. Final count: %ld\n", m_encoderCount);
    
    // Publish success event
    auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_OPENED_SUCCESS);
    m_eventBus.publish(BridgeEvent::BRIDGE_OPENED_SUCCESS, eventData);
    
    Serial.println("MOTOR CONTROL: Success event published");
}

void MotorControl::lowerBridge() {
    Serial.println("MOTOR CONTROL: Command received -> lowerBridge()");
    
    if (m_motorRunning) {
        Serial.println("MOTOR CONTROL: Motor already running, ignoring command");
        return;
    }
    
    // SIMULATION MODE: Skip actual motor control
    if (m_simulationMode) {
        Serial.println("MOTOR CONTROL: SIMULATION MODE - Simulating bridge lower");
        m_motorRunning = true;
        m_raisingBridge = false;
        
        // Simulate movement with a short delay
        delay(2000); // 2 seconds simulation delay
        
        m_motorRunning = false;
        Serial.println("MOTOR CONTROL: Bridge lowered successfully (SIMULATED)");
        
        // Publish success event
        auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_CLOSED_SUCCESS);
        m_eventBus.publish(BridgeEvent::BRIDGE_CLOSED_SUCCESS, eventData);
        
        Serial.println("MOTOR CONTROL: Success event published");
        return;
    }
    
    // REAL MODE: Actual motor control
    m_raisingBridge = false;
    m_motorRunning = true;
    
    Serial.println("MOTOR CONTROL: Starting to lower bridge");
    Serial.printf("MOTOR CONTROL: Current encoder count: %ld\n", m_encoderCount);
    
    // ENCODER-BASED CONTROL: Use encoder feedback for precise positioning
    long startCount = m_encoderCount;
    long targetCount = startCount - BRIDGE_TRAVEL_COUNTS; // Move by bridge travel distance
    
    Serial.printf("MOTOR CONTROL: Target count: %ld (travel: %d counts)\n", targetCount, BRIDGE_TRAVEL_COUNTS);
    
    // Start motor in reverse direction (adjust speed as needed)
    setMotorSpeed(180, false);
    
    // Monitor movement until target is reached
    unsigned long startTime = millis();
    unsigned long timeout = 15000; // 15 second timeout for safety
    
    while (m_motorRunning && !isTargetReached(targetCount) && (millis() - startTime) < timeout) {
        delay(10); // Small delay to prevent overwhelming the serial output
        
        // Print progress every 500ms
        if ((millis() - startTime) % 500 == 0) {
            Serial.printf("MOTOR CONTROL: Progress - Count: %ld, Target: %ld\n", m_encoderCount, targetCount);
        }
    }
    
    // Stop motor
    stopMotor();
    
    if (millis() - startTime >= timeout) {
        Serial.println("MOTOR CONTROL: Bridge lower operation timed out!");
        // TODO: In real implementation, trigger fault event
        return;
    }
    
    Serial.printf("MOTOR CONTROL: Bridge lowered successfully. Final count: %ld\n", m_encoderCount);
    
    // Publish success event
    auto* eventData = new SimpleEventData(BridgeEvent::BRIDGE_CLOSED_SUCCESS);
    m_eventBus.publish(BridgeEvent::BRIDGE_CLOSED_SUCCESS, eventData);
    
    Serial.println("MOTOR CONTROL: Success event published");
}

void MotorControl::halt() {
    Serial.println("MOTOR CONTROL: Emergency halt command received.");
    stopMotor();
    Serial.println("MOTOR CONTROL: Motor stopped immediately");
}

void MotorControl::testMotor() {
    Serial.println("MOTOR CONTROL: Starting motor test sequence...");
    
    // Test forward direction
    Serial.println("MOTOR CONTROL: Testing forward direction for 2 seconds");
    setMotorSpeed(100, true); // Low speed forward
    delay(2000);
    stopMotor();
    delay(1000);
    
    // Test reverse direction
    Serial.println("MOTOR CONTROL: Testing reverse direction for 2 seconds");
    setMotorSpeed(100, false); // Low speed reverse
    delay(2000);
    stopMotor();
    
    Serial.printf("MOTOR CONTROL: Test complete. Final encoder count: %ld\n", m_encoderCount);
}

void MotorControl::testEncoder() {
    Serial.println("MOTOR CONTROL: Testing encoder...");
    Serial.println("MOTOR CONTROL: Manually rotate the motor shaft and watch the count change");
    Serial.println("MOTOR CONTROL: Press any key to stop the test");
    
    long lastCount = m_encoderCount;
    unsigned long lastPrint = millis();
    
    while (!Serial.available()) {
        if (millis() - lastPrint > 200) { // Print every 200ms
            if (m_encoderCount != lastCount) {
                Serial.printf("MOTOR CONTROL: Encoder count: %ld (changed by %ld)\n", 
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
    
    Serial.printf("MOTOR CONTROL: Encoder test complete. Final count: %ld\n", m_encoderCount);
}

void MotorControl::checkSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();
        
        Serial.printf("MOTOR CONTROL: Received command: '%s'\n", command.c_str());
        
        if (command == "test motor" || command == "tm") {
            testMotor();
        }
        else if (command == "test encoder" || command == "te") {
            testEncoder();
        }
        else if (command == "raise" || command == "r") {
            raiseBridge();
        }
        else if (command == "lower" || command == "l") {
            lowerBridge();
        }
        else if (command == "halt" || command == "h" || command == "stop") {
            halt();
        }
        else if (command == "count" || command == "c") {
            Serial.printf("MOTOR CONTROL: Current encoder count: %ld\n", getEncoderCount());
        }
        else if (command == "reset" || command == "0") {
            resetEncoder();
        }
        else if (command == "sim on" || command == "simulation on") {
            m_simulationMode = true;
            Serial.println("MOTOR CONTROL: SIMULATION MODE ENABLED - Motor commands will be simulated");
        }
        else if (command == "sim off" || command == "simulation off") {
            m_simulationMode = false;
            Serial.println("MOTOR CONTROL: SIMULATION MODE DISABLED - Motor commands will use real hardware");
        }
        else if (command == "status" || command == "mode") {
            Serial.printf("MOTOR CONTROL: Current mode: %s\n", m_simulationMode ? "SIMULATION" : "REAL");
            Serial.printf("MOTOR CONTROL: Encoder count: %ld\n", m_encoderCount);
            Serial.printf("MOTOR CONTROL: Motor running: %s\n", m_motorRunning ? "YES" : "NO");
        }
        else if (command == "help" || command == "?") {
            Serial.println("MOTOR CONTROL: Available commands:");
            Serial.println("  'test motor' or 'tm' - Test motor movement");
            Serial.println("  'test encoder' or 'te' - Test encoder feedback");
            Serial.println("  'raise' or 'r' - Raise bridge");
            Serial.println("  'lower' or 'l' - Lower bridge");
            Serial.println("  'halt' or 'h' - Stop motor");
            Serial.println("  'count' or 'c' - Show encoder count");
            Serial.println("  'reset' or '0' - Reset encoder to 0");
            Serial.println("  'sim on' - Enable simulation mode");
            Serial.println("  'sim off' - Disable simulation mode");
            Serial.println("  'status' or 'mode' - Show current mode and status");
            Serial.println("  'help' or '?' - Show this help");
        }
        else {
            Serial.println("MOTOR CONTROL: Unknown command. Type 'help' for available commands.");
        }
    }
}

long MotorControl::getEncoderCount() {
    return m_encoderCount;
}

void MotorControl::resetEncoder() {
    m_encoderCount = 0;
    Serial.println("MOTOR CONTROL: Encoder count reset to 0");
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
    
    Serial.printf("MOTOR CONTROL: Motor set to speed %d, direction %s\n", 
                  speed, forward ? "FORWARD" : "REVERSE");
}

void MotorControl::stopMotor() {
    // Stop motor by setting both direction pins LOW and PWM to 0
    digitalWrite(MOTOR_DIR_PIN_1, LOW);
    digitalWrite(MOTOR_DIR_PIN_2, LOW);
    analogWrite(MOTOR_PWM_PIN, 0);
    
    m_motorRunning = false;
    
    Serial.println("MOTOR CONTROL: Motor stopped");
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