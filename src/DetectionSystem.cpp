#include <Arduino.h>

#include "DetectionSystem.h"
#include "Logger.h"

// ------------------- Configuration -------------------
// BOAT DETECTION MODE CONFIGURATION
// Toggle between beam break sensor and ultrasonic sensors for boat passage detection
// Set to true to use IR beam break sensor
// Set to false to use existing ultrasonic sensors
#define USE_BEAM_BREAK_SENSOR false

// IR Beam Break Sensor Pin Definitions
// Receiver output (white wire): LOW = beam broken (boat passing), HIGH = clear
static const int BEAM_BREAK_RECEIVER_PIN = 999;  // GPIO pin for receiver output (white wire)
// Note: Transmitter (red=5V, black=GND) and Receiver (red=5V, black=GND, white=OUT)

// ULTRASONIC SENSOR CONFIGURATION
// Left side sensor
static const int LEFT_TRIG_PIN = 32;
static const int LEFT_ECHO_PIN = 33;

// Right side sensor
static const int RIGHT_TRIG_PIN = 25;
static const int RIGHT_ECHO_PIN = 26;

// Distance thresholds in centimeters (short-range)
static const float FAR_CM = 30.0f;   // 30 cm
static const float NEAR_CM = 20.0f;  // 20 cm
static const float CLOSE_CM = 10.0f; // 10 cm
// Event threshold (critical distance) for BOAT_DETECTED/BOAT_PASSED
static const float DETECT_THRESHOLD_CM = CLOSE_CM;
static const float CLEAR_HYST_CM = 2.0f; // Hysteresis when clearing detection

// Timing
static const unsigned long SAMPLE_INTERVAL_MS = 100; // 10 Hz
static const unsigned long DETECT_HOLD_MS = 800;     // must stay within detect range to trigger
static const unsigned long PASSED_HOLD_MS = 1200;    // must clear detect range to report passed

// Filtering
static const float EMA_ALPHA = 0.5f; // 0..1, higher = less smoothing

// ---------------------------------------------------------------------------------

// Constructor initializing the EventBus reference
DetectionSystem::DetectionSystem(EventBus &eventBus)
    : m_eventBus(eventBus),
      boatDetected(false),
      boatDirection(BoatDirection::NONE) {}

// Initialization method
void DetectionSystem::begin()
{
    boatDetected = false;
    boatDirection = BoatDirection::NONE;

    // Initialize left sensor
    leftEmaDistanceCm = -1.0f;
    leftLastZone = -1;
    leftCriticalEnterMs = 0;

    // Initialize right sensor
    rightEmaDistanceCm = -1.0f;
    rightLastZone = -1;
    rightCriticalEnterMs = 0;

    // Clear timing values
    lastSampleMs = 0;
    passedCriticalEnterMs = 0;
    passedClearEnterMs = 0;
    
    // Initialise beam break tracking
    beamBroken = false;
    beamBrokenEnterMs = 0;
    beamClearEnterMs = 0;

    // Setup both ultrasonic sensors
    pinMode(LEFT_TRIG_PIN, OUTPUT);
    pinMode(LEFT_ECHO_PIN, INPUT);
    pinMode(RIGHT_TRIG_PIN, OUTPUT);
    pinMode(RIGHT_ECHO_PIN, INPUT);

    digitalWrite(LEFT_TRIG_PIN, LOW);
    digitalWrite(RIGHT_TRIG_PIN, LOW);
    
    // Setup beam break sensor if enabled
    #if USE_BEAM_BREAK_SENSOR
        pinMode(BEAM_BREAK_RECEIVER_PIN, INPUT);
        LOG_INFO(Logger::TAG_DS, "Beam break sensor initialized on pin %d", BEAM_BREAK_RECEIVER_PIN);
    #else
        LOG_INFO(Logger::TAG_DS, "Using ultrasonic sensors for boat passage detection");
    #endif
}

// Periodic update method
void DetectionSystem::update()
{
    const unsigned long now = millis();
    if (now - lastSampleMs < SAMPLE_INTERVAL_MS)
        return;
    lastSampleMs = now;

    // Read both sensors
    const float leftRawDist = readDistanceCm(LEFT_TRIG_PIN, LEFT_ECHO_PIN);
    const float rightRawDist = readDistanceCm(RIGHT_TRIG_PIN, RIGHT_ECHO_PIN);

    // Update filtered values
    updateFilteredDistances(leftRawDist, rightRawDist);

    // Update zone information
    updateZones();

    // Handle boat detection logic based on current state
    if (!boatDetected)
    {
        // Not currently tracking a boat, look for initial detection
        checkInitialDetection();
    }
    else
    {
        // Already tracking a boat, check for pass completion
        checkBoatPassed();
    }
}

// Read distance using an HC-SR04 compatible approach
float DetectionSystem::readDistanceCm(int trigPin, int echoPin)
{
    // Trigger pulse: LOW 2us, HIGH 10us, then LOW
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Echo pulse width in microseconds, with timeout to avoid blocking
    const unsigned long timeoutUs = 10000UL; // ~1.7 m max wait; our ranges are short
    unsigned long duration = pulseIn(echoPin, HIGH, timeoutUs);
    if (duration == 0)
        return -1.0f; // timeout / out-of-range

    // Convert to cm; HC-SR04 typical formula: cm = us / 58.0
    return (float)duration / 58.0f;
}

// Update the filtered distance values using exponential moving average
void DetectionSystem::updateFilteredDistances(float leftRawDist, float rightRawDist)
{
    if (leftRawDist > 0)
    {
        if (leftEmaDistanceCm < 0)
            leftEmaDistanceCm = leftRawDist;
        else
            leftEmaDistanceCm = EMA_ALPHA * leftRawDist + (1.0f - EMA_ALPHA) * leftEmaDistanceCm;
    }

    if (rightRawDist > 0)
    {
        if (rightEmaDistanceCm < 0)
            rightEmaDistanceCm = rightRawDist;
        else
            rightEmaDistanceCm = EMA_ALPHA * rightRawDist + (1.0f - EMA_ALPHA) * rightEmaDistanceCm;
    }
}

// Update zone classifications and log changes
void DetectionSystem::updateZones()
{
    // Get zones for both sensors
    int leftZone = getZoneFromDistance(leftEmaDistanceCm);
    int rightZone = getZoneFromDistance(rightEmaDistanceCm);

    // Log zone changes for left sensor
    if (leftZone != leftLastZone)
    {
        leftLastZone = leftZone;
        switch (leftZone)
        {
        case 0:
            LOG_DEBUG(Logger::TAG_DS, "LEFT SENSOR: Object detected (far)");
            break;
        case 1:
            LOG_DEBUG(Logger::TAG_DS, "LEFT SENSOR: Object approaching (near)");
            break;
        case 2:
            LOG_DEBUG(Logger::TAG_DS, "LEFT SENSOR: Object close");
            break;
        default:
            LOG_DEBUG(Logger::TAG_DS, "LEFT SENSOR: No object in range");
            break;
        }
    }

    // Log zone changes for right sensor
    if (rightZone != rightLastZone)
    {
        rightLastZone = rightZone;
        switch (rightZone)
        {
        case 0:
            LOG_DEBUG(Logger::TAG_DS, "RIGHT SENSOR: Object detected (far)");
            break;
        case 1:
            LOG_DEBUG(Logger::TAG_DS, "RIGHT SENSOR: Object approaching (near)");
            break;
        case 2:
            LOG_DEBUG(Logger::TAG_DS, "RIGHT SENSOR: Object close");
            break;
        default:
            LOG_DEBUG(Logger::TAG_DS, "RIGHT SENSOR: No object in range");
            break;
        }
    }
}

// Determine zone from distance measurement
int DetectionSystem::getZoneFromDistance(float distance) const
{
    int zone = 3; // none
    if (distance > 0)
    {
        if (distance <= CLOSE_CM)
            zone = 2; // close
        else if (distance <= NEAR_CM)
            zone = 1; // near
        else if (distance <= FAR_CM)
            zone = 0; // far
        else
            zone = 3; // none (too far)
    }
    return zone;
}

// Check if a value is in critical range
bool DetectionSystem::inCriticalRange(float cm) const
{
    return (cm > 0 && cm <= DETECT_THRESHOLD_CM);
}

// Look for initial boat detection from either sensor
void DetectionSystem::checkInitialDetection()
{
    const unsigned long now = millis();

    // Check left sensor first (priority doesn't matter)
    const bool leftCritical = inCriticalRange(leftEmaDistanceCm);
    if (leftCritical)
    {
        if (leftCriticalEnterMs == 0)
        {
            leftCriticalEnterMs = now;
            // Reset right sensor timer if left is detecting
            rightCriticalEnterMs = 0;
        }

        // If left sensor has been in critical range long enough
        if (now - leftCriticalEnterMs >= DETECT_HOLD_MS)
        {
            // Boat detected from left, traveling right
            boatDetected = true;
            boatDirection = BoatDirection::LEFT_TO_RIGHT;
            leftCriticalEnterMs = 0;

            if (!m_simulationMode)
            {
                LOG_INFO(Logger::TAG_DS, "LEFT SENSOR: BOAT_DETECTED (debounced) - Direction: LEFT TO RIGHT");
                // Replace this:
                // m_eventBus.publish(BridgeEvent::BOAT_DETECTED, nullptr, EventPriority::NORMAL);
                // With:
                auto* leftDetectedData = new BoatEventData(BridgeEvent::BOAT_DETECTED_LEFT, BoatEventSide::LEFT);
                m_eventBus.publish(BridgeEvent::BOAT_DETECTED_LEFT, leftDetectedData, EventPriority::NORMAL);

                auto* detectedData = new BoatEventData(BridgeEvent::BOAT_DETECTED, BoatEventSide::LEFT);
                m_eventBus.publish(BridgeEvent::BOAT_DETECTED, detectedData, EventPriority::NORMAL); // For backward compatibility
            }
            else
            {
                LOG_INFO(Logger::TAG_DS, "LEFT SENSOR: SIM MODE - detection suppressed");
            }
        }
    }
    else
    {
        // Reset timer if not in critical range
        leftCriticalEnterMs = 0;
    }

    // Only check right sensor if left hasn't triggered detection
    if (!leftCritical)
    {
        const bool rightCritical = inCriticalRange(rightEmaDistanceCm);
        if (rightCritical)
        {
            if (rightCriticalEnterMs == 0)
                rightCriticalEnterMs = now;

            // If right sensor has been in critical range long enough
            if (now - rightCriticalEnterMs >= DETECT_HOLD_MS)
            {
                // Boat detected from right, traveling left
                boatDetected = true;
                boatDirection = BoatDirection::RIGHT_TO_LEFT;
                rightCriticalEnterMs = 0;

                if (!m_simulationMode)
                {
                    LOG_INFO(Logger::TAG_DS, "RIGHT SENSOR: BOAT_DETECTED (debounced) - Direction: RIGHT TO LEFT");
                    // Replace this:
                    // m_eventBus.publish(BridgeEvent::BOAT_DETECTED, nullptr, EventPriority::NORMAL);
                    // With:
                    auto* rightDetectedData = new BoatEventData(BridgeEvent::BOAT_DETECTED_RIGHT, BoatEventSide::RIGHT);
                    m_eventBus.publish(BridgeEvent::BOAT_DETECTED_RIGHT, rightDetectedData, EventPriority::NORMAL);

                    auto* detectedData = new BoatEventData(BridgeEvent::BOAT_DETECTED, BoatEventSide::RIGHT);
                    m_eventBus.publish(BridgeEvent::BOAT_DETECTED, detectedData, EventPriority::NORMAL); // For backward compatibility
                }
                else
                {
                    LOG_INFO(Logger::TAG_DS, "RIGHT SENSOR: SIM MODE - detection suppressed");
                }
            }
        }
        else
        {
            // Reset timer if not in critical range
            rightCriticalEnterMs = 0;
        }
    }
}

// Check if boat has passed through and exited on the other side
void DetectionSystem::checkBoatPassed()
{
    const unsigned long now = millis();

    #if USE_BEAM_BREAK_SENSOR
        // Use beam break sensor for passage detection (direction-independent)
        const bool currentBeamBroken = readBeamBreak();
        
        if (currentBeamBroken)
        {
            // Beam is broken (boat is passing through)
            if (!beamBroken)
            {
                // Beam just broke
                beamBroken = true;
                beamBrokenEnterMs = now;
                beamClearEnterMs = 0;
                LOG_INFO(Logger::TAG_DS, "BEAM BREAK: Boat passing through");
            }
        }
        else
        {
            // Beam is clear
            if (beamBroken)
            {
                // Beam was broken, now clear.. boat has passed
                if (beamClearEnterMs == 0)
                {
                    beamClearEnterMs = now;
                }
                
                // Debounce the clear signal (100ms)
                if (now - beamClearEnterMs >= 100)
                {
                    // Boat has fully passed
                    boatDetected = false;
                    BoatEventSide passedSide = (boatDirection == BoatDirection::LEFT_TO_RIGHT) ? BoatEventSide::RIGHT : BoatEventSide::LEFT;
                    boatDirection = BoatDirection::NONE;
                    beamBroken = false;
                    beamBrokenEnterMs = 0;
                    beamClearEnterMs = 0;
                    
                    if (!m_simulationMode)
                    {
                        LOG_INFO(Logger::TAG_DS, "BEAM BREAK: BOAT_PASSED (debounced)");
                        
                        // Publish side-specific event
                        if (passedSide == BoatEventSide::RIGHT) {
                            auto* rightPassedData = new BoatEventData(BridgeEvent::BOAT_PASSED_RIGHT, BoatEventSide::RIGHT);
                            m_eventBus.publish(BridgeEvent::BOAT_PASSED_RIGHT, rightPassedData, EventPriority::NORMAL);
                        } else {
                            auto* leftPassedData = new BoatEventData(BridgeEvent::BOAT_PASSED_LEFT, BoatEventSide::LEFT);
                            m_eventBus.publish(BridgeEvent::BOAT_PASSED_LEFT, leftPassedData, EventPriority::NORMAL);
                        }
                        
                        // Publish general event
                        auto* passedData = new BoatEventData(BridgeEvent::BOAT_PASSED, passedSide);
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED, passedData, EventPriority::NORMAL);
                    }
                    else
                    {
                        LOG_INFO(Logger::TAG_DS, "BEAM BREAK: SIM MODE - passed event suppressed");
                    }
                }
            }
        }
    
    #else
        // Use ultrasonic sensors for passage detection (direction-aware)
        
        // Handle boat passing detection based on direction
        if (boatDirection == BoatDirection::LEFT_TO_RIGHT)
        {
            // Boat initially detected on left, now check right sensor
            const bool rightCritical = inCriticalRange(rightEmaDistanceCm);

            if (rightCritical)
            {
                // Boat is at the right sensor, start timing
                if (passedCriticalEnterMs == 0)
                {
                    passedCriticalEnterMs = now;
                    LOG_INFO(Logger::TAG_DS, "RIGHT SENSOR: Boat passing through");
                }
                // Reset clear timer as boat is still detected
                passedClearEnterMs = 0;
            }
            else if (passedCriticalEnterMs > 0)
            {
                // Boat was at right sensor but now has cleared it
                if (passedClearEnterMs == 0)
                    passedClearEnterMs = now;

                // Need to wait for boat to fully clear the sensor
                if (now - passedClearEnterMs >= PASSED_HOLD_MS)
                {
                    // Boat has fully passed
                    boatDetected = false;
                    boatDirection = BoatDirection::NONE;
                    passedCriticalEnterMs = 0;
                    passedClearEnterMs = 0;

                    if (!m_simulationMode)
                    {
                        LOG_INFO(Logger::TAG_DS, "RIGHT SENSOR: BOAT_PASSED (debounced)");
                        auto* rightPassedData = new BoatEventData(BridgeEvent::BOAT_PASSED_RIGHT, BoatEventSide::RIGHT);
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED_RIGHT, rightPassedData, EventPriority::NORMAL);

                        auto* passedData = new BoatEventData(BridgeEvent::BOAT_PASSED, BoatEventSide::RIGHT);
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED, passedData, EventPriority::NORMAL); // For backward compatibility
                    }
                    else
                    {
                        LOG_INFO(Logger::TAG_DS, "RIGHT SENSOR: SIM MODE - passed event suppressed");
                    }
                }
            }
        }
        else if (boatDirection == BoatDirection::RIGHT_TO_LEFT)
        {
            // Boat initially detected on right, now check left sensor
            const bool leftCritical = inCriticalRange(leftEmaDistanceCm);

            if (leftCritical)
            {
                // Boat is at the left sensor, start timing
                if (passedCriticalEnterMs == 0)
                {
                    passedCriticalEnterMs = now;
                    LOG_INFO(Logger::TAG_DS, "LEFT SENSOR: Boat passing through");
                }
                // Reset clear timer as boat is still detected
                passedClearEnterMs = 0;
            }
            else if (passedCriticalEnterMs > 0)
            {
                // Boat was at left sensor but now has cleared it
                if (passedClearEnterMs == 0)
                    passedClearEnterMs = now;

                // Need to wait for boat to fully clear the sensor
                if (now - passedClearEnterMs >= PASSED_HOLD_MS)
                {
                    // Boat has fully passed
                    boatDetected = false;
                    boatDirection = BoatDirection::NONE;
                    passedCriticalEnterMs = 0;
                    passedClearEnterMs = 0;

                    if (!m_simulationMode)
                    {
                        LOG_INFO(Logger::TAG_DS, "LEFT SENSOR: BOAT_PASSED (debounced)");
                        auto* leftPassedData = new BoatEventData(BridgeEvent::BOAT_PASSED_LEFT, BoatEventSide::LEFT);
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED_LEFT, leftPassedData, EventPriority::NORMAL);

                        auto* passedData = new BoatEventData(BridgeEvent::BOAT_PASSED, BoatEventSide::LEFT);
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED, passedData, EventPriority::NORMAL); // For backward compatibility
                    }
                    else
                    {
                        LOG_INFO(Logger::TAG_DS, "LEFT SENSOR: SIM MODE - passed event suppressed");
                    }
                }
            }
        }
    #endif
}

// Check if the system has been initialized
bool DetectionSystem::isInitialized() const
{
    return (leftEmaDistanceCm > 0) || (rightEmaDistanceCm > 0);
}

void DetectionSystem::setSimulationMode(bool enable)
{
    m_simulationMode = enable;
    LOG_INFO(Logger::TAG_DS, "ULTRASONIC: Simulation mode %s", enable ? "ENABLED" : "DISABLED");
}

bool DetectionSystem::isSimulationMode() const
{
    return m_simulationMode;
}

// Getter methods for UI/monitoring
float DetectionSystem::getLeftFilteredDistanceCm() const
{
    return leftEmaDistanceCm;
}

float DetectionSystem::getRightFilteredDistanceCm() const
{
    return rightEmaDistanceCm;
}

int DetectionSystem::getLeftZoneIndex() const
{
    return (leftLastZone < 0 ? 3 : leftLastZone);
}

int DetectionSystem::getRightZoneIndex() const
{
    return (rightLastZone < 0 ? 3 : rightLastZone);
}

const char *DetectionSystem::getLeftZoneName() const
{
    switch (getLeftZoneIndex())
    {
    case 0:
        return "far";
    case 1:
        return "near";
    case 2:
        return "close";
    default:
        return "none";
    }
}

const char *DetectionSystem::getRightZoneName() const
{
    switch (getRightZoneIndex())
    {
    case 0:
        return "far";
    case 1:
        return "near";
    case 2:
        return "close";
    default:
        return "none";
    }
}

DetectionSystem::BoatDirection DetectionSystem::getCurrentDirection() const
{
    return boatDirection;
}

const char *DetectionSystem::getDirectionName() const
{
    switch (boatDirection)
    {
    case BoatDirection::LEFT_TO_RIGHT:
        return "left-to-right";
    case BoatDirection::RIGHT_TO_LEFT:
        return "right-to-left";
    default:
        return "none";
    }
}

bool DetectionSystem::readBeamBreak() const
{
    #if USE_BEAM_BREAK_SENSOR
        // Read beam break sensor
        // Output is LOW when beam is broken (boat passing), HIGH when clear
        int reading = digitalRead(BEAM_BREAK_RECEIVER_PIN);
        return (reading == LOW);  // true = beam broken
    #else
        return false;  // Not using beam break sensor
    #endif
}
