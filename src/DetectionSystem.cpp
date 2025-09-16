#include <Arduino.h>

#include "DetectionSystem.h"

// ------------------- Configuration -------------------
// Pins
static const int TRIG_PIN = 32;
static const int ECHO_PIN = 33;

// Distance thresholds in centimeters (short-range)
static const float FAR_CM    = 30.0f;  // 30 cm
static const float NEAR_CM   = 20.0f;  // 20 cm
static const float CLOSE_CM  = 10.0f;  // 10 cm
// Event threshold (critical distance) for BOAT_DETECTED/BOAT_PASSED
static const float DETECT_THRESHOLD_CM = CLOSE_CM;
static const float CLEAR_HYST_CM = 2.0f;    // Hysteresis when clearing detection

// Timing
static const unsigned long SAMPLE_INTERVAL_MS = 100;   // 10 Hz
static const unsigned long DETECT_HOLD_MS     = 800;   // must stay within detect range to trigger
static const unsigned long PASSED_HOLD_MS     = 1200;  // must clear detect range to report passed

// Filtering
static const float EMA_ALPHA = 0.5f; // 0..1, higher = less smoothing

// ---------------------------------------------------------------------------------

// Constructor initializing the EventBus reference
DetectionSystem::DetectionSystem(EventBus& eventBus)
    : m_eventBus(eventBus), boatDetected(false) {}

// Initialization method
void DetectionSystem::begin() {
    boatDetected = false;
    emaDistanceCm = -1.0f;
    lastZone = -1;
    lastSampleMs = 0;
    criticalEnterMs = 0;
    clearEnterMs = 0;

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);
}

// Periodic update method
void DetectionSystem::update() {
    const unsigned long now = millis();
    if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
    lastSampleMs = now;
    detectBoat();
}

// Read distance using an HC-SR04 compatible approach
float DetectionSystem::readDistanceCm() {
    // Trigger pulse: LOW 2us, HIGH 10us, then LOW
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Echo pulse width in microseconds, with timeout to avoid blocking
    const unsigned long timeoutUs = 10000UL; // ~1.7 m max wait; our ranges are short
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, timeoutUs);
    if (duration == 0) return -1.0f; // timeout / out-of-range

    // Convert to cm; HC-SR04 typical formula: cm = us / 58.0
    return (float)duration / 58.0f;
}

bool DetectionSystem::inCriticalRange(float cm) const {
    return (cm > 0 && cm <= DETECT_THRESHOLD_CM);
}

// Classify + debounce + publish events
void DetectionSystem::detectBoat() {
    const unsigned long now = millis();
    const float dRaw = readDistanceCm();

    // Update filtered value
    if (dRaw > 0) {
        if (emaDistanceCm < 0) emaDistanceCm = dRaw;
        else emaDistanceCm = EMA_ALPHA * dRaw + (1.0f - EMA_ALPHA) * emaDistanceCm;
    }

    // Determine zone from filtered distance (if invalid, treat as none)
    int zone = 3; // none
    const float d = emaDistanceCm;
    if (d > 0) {
        if (d <= CLOSE_CM) zone = 2;           // close
        else if (d <= NEAR_CM) zone = 1;       // near
        else if (d <= FAR_CM) zone = 0;        // far
        else zone = 3;                         // none (too far)
    }

    if (zone != lastZone) {
        lastZone = zone;
        switch (zone) {
            case 0: Serial.println("ULTRASONIC: Object detected (far)"); break;
            case 1: Serial.println("ULTRASONIC: Object approaching (near)"); break;
            case 2: Serial.println("ULTRASONIC: Object close"); break;
            default: Serial.println("ULTRASONIC: No object in range"); break;
        }
    }

    // Debounced event logic
    const bool crit = inCriticalRange(d);
    if (crit) {
        clearEnterMs = 0;
        if (!boatDetected) {
            if (criticalEnterMs == 0) criticalEnterMs = now;
            if (now - criticalEnterMs >= DETECT_HOLD_MS) {
                boatDetected = true;
                criticalEnterMs = 0;
                if (!m_simulationMode) {
                    Serial.println("ULTRASONIC: BOAT_DETECTED (debounced)");
                    m_eventBus.publish(BridgeEvent::BOAT_DETECTED, nullptr, EventPriority::NORMAL);
                } else {
                    Serial.println("ULTRASONIC: SIM MODE - detection suppressed");
                }
            }
        }
    } else {
        criticalEnterMs = 0;
        if (boatDetected) {
            if (clearEnterMs == 0) clearEnterMs = now;
            // Require it to clear threshold plus hysteresis to avoid chatter
            if (d < 0 || d > (DETECT_THRESHOLD_CM + CLEAR_HYST_CM)) {
                if (now - clearEnterMs >= PASSED_HOLD_MS) {
                    boatDetected = false;
                    clearEnterMs = 0;
                    if (!m_simulationMode) {
                        Serial.println("ULTRASONIC: BOAT_PASSED (debounced)");
                        m_eventBus.publish(BridgeEvent::BOAT_PASSED, nullptr, EventPriority::NORMAL);
                    } else {
                        Serial.println("ULTRASONIC: SIM MODE - clear suppressed");
                    }
                }
            } else {
                // Still too close; reset the clear timer
                clearEnterMs = 0;
            }
        }
    }
}

// New method to check if the system has been initialized
bool DetectionSystem::isInitialized() const {
    // Consider initialized once first update has run (emaDistanceCm set) or detection latched
    return boatDetected || (emaDistanceCm > 0);
}

void DetectionSystem::setSimulationMode(bool enable) {
    m_simulationMode = enable;
    Serial.printf("ULTRASONIC: Simulation mode %s\n", enable ? "ENABLED" : "DISABLED");
}

bool DetectionSystem::isSimulationMode() const { return m_simulationMode; }

float DetectionSystem::getFilteredDistanceCm() const { return emaDistanceCm; }

int DetectionSystem::getZoneIndex() const { return (lastZone < 0 ? 3 : lastZone); }

const char* DetectionSystem::zoneName() const {
    switch (getZoneIndex()) {
        case 0: return "far";
        case 1: return "near";
        case 2: return "close";
        default: return "none";
    }
}
