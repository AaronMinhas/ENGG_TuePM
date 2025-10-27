#include <Arduino.h>

#include "DetectionSystem.h"
#include "Logger.h"

// ------------------- Configuration -------------------
// Boat passage clearance relies on the beam break sensor; ultrasonic sensors
// continue providing directional detection for queue management.

// IR Beam Break Sensor Pin Definitions
// Receiver output (white wire): LOW = beam broken (boat passing), HIGH = clear
static const int BEAM_BREAK_RECEIVER_PIN = 34;  // GPIO pin for receiver output (white wire)
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
// Timing
static const unsigned long SAMPLE_INTERVAL_MS = 100; // 10 Hz
static const unsigned long DETECT_HOLD_MS = 800;     // must stay within detect range to trigger

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
    leftPrevZone = 3;
    leftCriticalEnterMs = 0;
    leftApproachActive = false;

    // Initialize right sensor
    rightEmaDistanceCm = -1.0f;
    rightLastZone = -1;
    rightPrevZone = 3;
    rightCriticalEnterMs = 0;
    rightApproachActive = false;

    // Clear timing values
    lastSampleMs = 0;
    // Initialise beam break tracking
    beamBroken = false;
    beamBrokenEnterMs = 0;
    beamClearEnterMs = 0;
    pendingBoatDirections.clear();
    pendingPriorityDirection = BoatDirection::NONE;

    // Setup both ultrasonic sensors
    pinMode(LEFT_TRIG_PIN, OUTPUT);
    pinMode(LEFT_ECHO_PIN, INPUT);
    pinMode(RIGHT_TRIG_PIN, OUTPUT);
    pinMode(RIGHT_ECHO_PIN, INPUT);

    digitalWrite(LEFT_TRIG_PIN, LOW);
    digitalWrite(RIGHT_TRIG_PIN, LOW);
    
    // Setup beam break sensor (receiver output pin only)
    pinMode(BEAM_BREAK_RECEIVER_PIN, INPUT);
    LOG_INFO(Logger::TAG_DS, "Beam break sensor initialised on pin %d", BEAM_BREAK_RECEIVER_PIN);
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

    // Handle detection and passage tracking
    checkInitialDetection();
    checkBoatPassed();
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

    int prevLeftZone = (leftLastZone < 0) ? 3 : leftLastZone;
    int prevRightZone = (rightLastZone < 0) ? 3 : rightLastZone;

    // Log zone changes for left sensor
    if (leftZone != prevLeftZone)
    {
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
    leftPrevZone = prevLeftZone;
    leftLastZone = leftZone;

    // Log zone changes for right sensor
    if (rightZone != prevRightZone)
    {
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
    rightPrevZone = prevRightZone;
    rightLastZone = rightZone;
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

    auto handleDetection = [&](const char* sensorName,
                               BoatDirection direction,
                               BoatEventSide eventSide,
                               BridgeEvent sideEvent) {
        if (!m_simulationMode)
        {
            LOG_INFO(Logger::TAG_DS, "%s SENSOR: BOAT_DETECTED (debounced) - Direction: %s TO %s",
                     sensorName,
                     (direction == BoatDirection::LEFT_TO_RIGHT) ? "LEFT" : "RIGHT",
                     (direction == BoatDirection::LEFT_TO_RIGHT) ? "RIGHT" : "LEFT");

            auto* sideDetectedData = new BoatEventData(sideEvent, eventSide);
            m_eventBus.publish(sideEvent, sideDetectedData, EventPriority::NORMAL);

            auto* detectedData = new BoatEventData(BridgeEvent::BOAT_DETECTED, eventSide);
            m_eventBus.publish(BridgeEvent::BOAT_DETECTED, detectedData, EventPriority::NORMAL); // Backward compatibility
        }
        else
        {
            LOG_INFO(Logger::TAG_DS, "%s SENSOR: SIM MODE - detection suppressed", sensorName);
        }

        if (!boatDetected)
        {
            boatDetected = true;
            boatDirection = direction;
            pendingPriorityDirection = BoatDirection::NONE;
        }
        else
        {
            bool duplicate = !pendingBoatDirections.empty() && pendingBoatDirections.back() == direction;
            if (!duplicate)
            {
                pendingBoatDirections.push_back(direction);
                LOG_INFO(Logger::TAG_DS, "%s SENSOR: Detection queued while boat in progress (queue length=%u)",
                         sensorName, static_cast<unsigned int>(pendingBoatDirections.size()));
            }
        }
    };

    auto processSensor = [&](float distanceCm,
                             int currentZone,
                             int previousZone,
                             bool& approachActive,
                             unsigned long& criticalEnterMs,
                             const char* sensorName,
                             BoatDirection direction,
                             BoatEventSide eventSide,
                             BridgeEvent sideEvent) {
        const bool isPriority = (pendingPriorityDirection == direction);

        if (!approachActive)
        {
            if ((currentZone <= 1 && previousZone >= 2) ||
                (isPriority && currentZone <= 2 && currentZone >= 0))
            {
                approachActive = true;
                criticalEnterMs = 0;
            }
        }
        else
        {
            if (currentZone == 3)
            {
                approachActive = false;
                criticalEnterMs = 0;
            }
        }

        const bool critical = inCriticalRange(distanceCm);
        if (!critical)
        {
            criticalEnterMs = 0;
            return;
        }

        if (!approachActive)
        {
            return;
        }

        if (criticalEnterMs == 0)
        {
            criticalEnterMs = now;
        }

        if (now - criticalEnterMs >= DETECT_HOLD_MS)
        {
            approachActive = false;
            criticalEnterMs = 0;

            if (isPriority)
            {
                pendingPriorityDirection = BoatDirection::NONE;
            }

            handleDetection(sensorName, direction, eventSide, sideEvent);
        }
    };

    const int currentLeftZone = (leftLastZone < 0) ? 3 : leftLastZone;
    processSensor(leftEmaDistanceCm,
                  currentLeftZone,
                  leftPrevZone,
                  leftApproachActive,
                  leftCriticalEnterMs,
                  "LEFT",
                  BoatDirection::LEFT_TO_RIGHT,
                  BoatEventSide::LEFT,
                  BridgeEvent::BOAT_DETECTED_LEFT);

    const int currentRightZone = (rightLastZone < 0) ? 3 : rightLastZone;
    processSensor(rightEmaDistanceCm,
                  currentRightZone,
                  rightPrevZone,
                  rightApproachActive,
                  rightCriticalEnterMs,
                  "RIGHT",
                  BoatDirection::RIGHT_TO_LEFT,
                  BoatEventSide::RIGHT,
                  BridgeEvent::BOAT_DETECTED_RIGHT);
}

// Check if boat has passed through and exited on the other side
void DetectionSystem::checkBoatPassed()
{
    const unsigned long now = millis();

    const bool currentBeamBroken = readBeamBreak();

    if (currentBeamBroken)
    {
        // Beam is broken (boat is crossing the span)
        if (!beamBroken)
        {
            beamBroken = true;
            beamBrokenEnterMs = now;
            beamClearEnterMs = 0;

            if (!m_simulationMode)
            {
                LOG_INFO(Logger::TAG_DS, "BEAM BREAK: Boat occupying channel");
                auto* activeData = new SimpleEventData(BridgeEvent::BEAM_BREAK_ACTIVE);
                m_eventBus.publish(BridgeEvent::BEAM_BREAK_ACTIVE, activeData, EventPriority::EMERGENCY);
            }
        }
        return;
    }

    // Beam currently clear
    if (!beamBroken)
    {
        beamClearEnterMs = 0;
        return;
    }

    if (beamClearEnterMs == 0)
    {
        beamClearEnterMs = now;
        return;
    }

    // Require a short debounce period to ensure the hull has cleared
    if (now - beamClearEnterMs < 100)
    {
        return;
    }

    beamBroken = false;
    beamBrokenEnterMs = 0;
    beamClearEnterMs = 0;

    boatDetected = false;
    BoatEventSide passedSide;
    if (boatDirection == BoatDirection::LEFT_TO_RIGHT)
    {
        passedSide = BoatEventSide::RIGHT;
    }
    else if (boatDirection == BoatDirection::RIGHT_TO_LEFT)
    {
        passedSide = BoatEventSide::LEFT;
    }
    else
    {
        passedSide = BoatEventSide::LEFT;
        LOG_WARN(Logger::TAG_DS, "BEAM BREAK: Boat direction unknown when clearing - defaulting to LEFT exit");
    }
    boatDirection = BoatDirection::NONE;

    if (!m_simulationMode)
    {
        LOG_INFO(Logger::TAG_DS, "BEAM BREAK: Boat clear (debounced)");

        auto* clearData = new SimpleEventData(BridgeEvent::BEAM_BREAK_CLEAR);
        m_eventBus.publish(BridgeEvent::BEAM_BREAK_CLEAR, clearData, EventPriority::EMERGENCY);

        // Publish side-specific event
        if (passedSide == BoatEventSide::RIGHT)
        {
            auto* rightPassedData = new BoatEventData(BridgeEvent::BOAT_PASSED_RIGHT, BoatEventSide::RIGHT);
            m_eventBus.publish(BridgeEvent::BOAT_PASSED_RIGHT, rightPassedData, EventPriority::NORMAL);
        }
        else
        {
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

    if (!pendingBoatDirections.empty())
    {
        pendingPriorityDirection = pendingBoatDirections.front();
        pendingBoatDirections.pop_front();
        LOG_INFO(Logger::TAG_DS, "Queued boat detected earlier (%s to %s) - awaiting sensor reconfirmation (remaining queue length=%u)",
                 (pendingPriorityDirection == BoatDirection::LEFT_TO_RIGHT) ? "LEFT" : "RIGHT",
                 (pendingPriorityDirection == BoatDirection::LEFT_TO_RIGHT) ? "RIGHT" : "LEFT",
                 static_cast<unsigned int>(pendingBoatDirections.size()));
    }
    else
    {
        pendingPriorityDirection = BoatDirection::NONE;
    }
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
    // Output is LOW when beam is broken (boat passing), HIGH when clear
    int reading = digitalRead(BEAM_BREAK_RECEIVER_PIN);
    return (reading == LOW);  // true = beam broken
}
