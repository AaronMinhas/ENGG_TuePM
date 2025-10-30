#include "MotorControl.h"
#include <Arduino.h>
#include "EventBus.h"
#include "Logger.h"

MotorControl::MotorControl(EventBus& eventBus)
    : m_eventBus(eventBus), 
      m_motorRunning(false),
      m_raisingBridge(false),
      m_simulationMode(false),      // Start in real mode by default
      m_limitCleared(false),
      m_inGracePeriod(false),
      m_graceEndsAt(0) {
}

void MotorControl::init() {
    LOG_INFO(Logger::TAG_MC, "Initialising motor control...");
    
    // Configure motor control pins
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    pinMode(MOTOR_DIR_PIN_1, OUTPUT);
    pinMode(MOTOR_DIR_PIN_2, OUTPUT);
    
    // Configure shared limit switch input on GPIO13 with internal pull-up
    pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);
    int initialLimitState = digitalRead(LIMIT_SWITCH_PIN);
    LOG_INFO(Logger::TAG_MC, "Limit switch initial state: %s",
             initialLimitState == LIMIT_SWITCH_ACTIVE_STATE ? "ACTIVE" : "INACTIVE");
    
    // Initialise motor to stopped state
    stopMotor();
    
    LOG_INFO(Logger::TAG_MC, "Initialisation complete");
    LOG_INFO(Logger::TAG_MC, "Using pins - PWM: %d, DIR1: %d, DIR2: %d, LIMIT: %d",
             MOTOR_PWM_PIN, MOTOR_DIR_PIN_1, MOTOR_DIR_PIN_2, LIMIT_SWITCH_PIN);
}

void MotorControl::raiseBridge() {
    LOG_INFO(Logger::TAG_MC, "Command received -> raiseBridge()");
    
    if (m_motorRunning) {
        LOG_WARN(Logger::TAG_MC, "Motor already running, ignoring command");
        return;
    }
    
    m_raisingBridge = true;
    m_motorRunning = true;
    m_limitCleared = !isLimitSwitchActive();
    m_inGracePeriod = false;
    m_graceEndsAt = 0;

    if (!m_limitCleared) {
        LOG_DEBUG(Logger::TAG_MC, "Starting raise with limit engaged - waiting for release before honouring stops");
    }

    // Start motor in forward direction (adjust speed as needed)
    setMotorSpeed(180, true); // 180/255 = ~70% speed, forward direction
    LOG_INFO(Logger::TAG_MC, "Motor raising bridge; monitoring shared limit switch for stop condition");
}

void MotorControl::lowerBridge() {
    LOG_INFO(Logger::TAG_MC, "Command received -> lowerBridge()");
    
    if (m_motorRunning) {
        LOG_WARN(Logger::TAG_MC, "Motor already running, ignoring command");
        return;
    }
    
    m_raisingBridge = false;
    m_motorRunning = true;
    m_limitCleared = !isLimitSwitchActive();
    m_inGracePeriod = false;
    m_graceEndsAt = 0;

    if (!m_limitCleared) {
        LOG_DEBUG(Logger::TAG_MC, "Starting lower with limit engaged - waiting for release before honouring stops");
    }

    // Start motor in reverse direction (adjust speed as needed)
    setMotorSpeed(180, false);
    LOG_INFO(Logger::TAG_MC, "Motor lowering bridge; monitoring shared limit switch for stop condition");
}

void MotorControl::checkProgress() {
    if (!m_motorRunning) {
        return; // No operation in progress
    }

    const bool limitActive = isLimitSwitchActive();
    const unsigned long now = millis();

    if (!limitActive) {
        if (!m_limitCleared) {
            m_limitCleared = true;
            LOG_DEBUG(Logger::TAG_MC, "Shared limit switch released - arming grace window");
        }

        if (!m_inGracePeriod) {
            m_inGracePeriod = true;
            m_graceEndsAt = now + LIMIT_RELEASE_GRACE_MS;
            LOG_DEBUG(Logger::TAG_MC, "Ignoring limit switch re-triggers for %lu ms",
                      LIMIT_RELEASE_GRACE_MS);
        }

        return;
    }

    // If we have not seen the limit release yet, keep running (still on the original switch)
    if (!m_limitCleared) {
        return;
    }

    // Honour grace window to prevent chatter stopping the motor immediately after release
    if (m_inGracePeriod && (long)(now - m_graceEndsAt) < 0) {
        return;
    }

    m_inGracePeriod = false;

    stopMotor();
    LOG_INFO(Logger::TAG_MC, "Limit switch re-engaged - stopping motor");

    BridgeEvent eventType = m_raisingBridge ? BridgeEvent::BRIDGE_OPENED_SUCCESS
                                            : BridgeEvent::BRIDGE_CLOSED_SUCCESS;
    auto* eventData = new SimpleEventData(eventType);
    m_eventBus.publish(eventType, eventData);
    LOG_DEBUG(Logger::TAG_MC, "Success event published due to limit switch");
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

    LOG_INFO(Logger::TAG_MC, "Motor test complete");
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
    m_limitCleared = false;
    m_inGracePeriod = false;
    m_graceEndsAt = 0;
    
    LOG_INFO(Logger::TAG_MC, "Motor stopped");
}
