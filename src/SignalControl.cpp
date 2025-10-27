#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include <Arduino.h>
#include "Logger.h"

/*
  Module: SignalControl
  Purpose:
    - Drive the physical lights for road traffic (car) and boats.
    - Provide high-level actions used by the FSM (stop/resume traffic, emergency halt).
    - Provide low-level per-side commands for UI (set left/right light to a color).
    - Publish success events so the FSM/StateWriter/UI can progress and reflect state.

  Safety:
    - Default to a safe state (all RED) whenever possible.
    - Ensure only one car aspect (R/Y/G) is active on a side at a time.

  Integration:
    - Publishes:
        TRAFFIC_STOPPED_SUCCESS
        TRAFFIC_RESUMED_SUCCESS
        CAR_LIGHT_CHANGED_SUCCESS
        BOAT_LIGHT_CHANGED_SUCCESS
    - LightChangeData ownership should be managed by EventBus/subscribers to avoid leaks.
*/

namespace {
  // Set to false if your LEDs are active-low (i.e., LOW = ON).
  constexpr bool ACTIVE_HIGH = true;

  // Car traffic lights: single group with Red/Yellow/Green pins.
  struct RGB { uint8_t r, y, g; };
  // Boat lights: each side has Red/Green pins.
  struct RG  { uint8_t r, y, g;  };

  // Car light group 
  RGB CAR {21, 19, 18};

  // Boat lights (R/G) per side
  RG  BOAT_LEFT  {4, 2, 15};
  RG  BOAT_RIGHT {5, 17, 16};

  // Internal flag to run one-time pin setup.
  bool pinsReady = false;

  // Drive a single pin ON/OFF according to ACTIVE_HIGH policy.
  inline void writePin(uint8_t pin, bool on) {
    if (pin == 255) return;
    digitalWrite(pin, (on == ACTIVE_HIGH) ? HIGH : LOW);
  }
  // Prepare a pin as OUTPUT and set it to OFF (safe default).
  inline void prep(uint8_t p){ 
    if (p!=255){ pinMode(p, OUTPUT); writePin(p,false);} 
}

  // Ensure all pins are initialized once before use.
  void ensurePins() {
    if (pinsReady) return;
    // Car (single group)
    prep(CAR.r);  prep(CAR.y);  prep(CAR.g);
    // Boat
    prep(BOAT_LEFT.r);  prep(BOAT_LEFT.g);
    prep(BOAT_RIGHT.r); prep(BOAT_RIGHT.g);
    pinsReady = true;
  }

  // Convenience: lowercase a String for case-insensitive comparisons.
  inline String lower(String s){ s.toLowerCase(); return s; }

  // Drive one car side to a specific aspect; exactly one of R/Y/G will be ON.
  void driveCar(const RGB& p, const String& color) {
    const auto c = lower(color);
    const bool R = (c=="red"), Y = (c=="yellow"), G = (c=="green");
    writePin(p.r, R); writePin(p.y, Y); writePin(p.g, G);
  }

  // Drive one boat side: Green OR Red (mutually exclusive).
  void driveBoat(const RG& p, const String& color) {
    const bool green = (lower(color)=="green");
    writePin(p.r, !green); writePin(p.g, green);
  }

  // Safe default for the whole system: everything to RED.
  void allSafeRed() {
    driveCar(CAR,  "Red");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
  }
} 

SignalControl::SignalControl(EventBus& eventBus) 
    : m_eventBus(eventBus),
      m_currentOperation(Operation::NONE),
      m_stopPhase(StopPhase::COMPLETE),
      m_resumePhase(ResumePhase::COMPLETE),
      m_operationStartTime(0),
      m_boatQueueActive(false),
      m_boatQueueStartTime(0),
      m_boatQueueSide("") {
    LOG_INFO(Logger::TAG_SC, "Initialised");
}

void SignalControl::begin() {
    ensurePins();
    // Default bridge state on boot: allow road traffic, hold boats.
    LOG_INFO(Logger::TAG_SC, "Applying default signal state (car=GREEN; boats=RED)");
    driveCar(CAR, "Green");
    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
}

void SignalControl::stopTraffic() {
    ensurePins();
    
    // Start phased stopping operation
    m_currentOperation = Operation::STOPPING_TRAFFIC;
    m_stopPhase = StopPhase::YELLOW_WARNING;
    m_operationStartTime = millis();
    
    // Green→Yellow (warning begins)
    LOG_INFO(Logger::TAG_SC, "Stopping traffic - Phase 1: car=YELLOW (warning)");
    driveCar(CAR, "Yellow");
    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
    
    // SUCCESS event will be published by update() after 6 seconds
}

void SignalControl::resumeTraffic() {
    ensurePins();
    
    // Start delayed resume operation
    m_currentOperation = Operation::RESUMING_TRAFFIC;
    m_resumePhase = ResumePhase::RED_WAITING;
    m_operationStartTime = millis();
    
    // T+0s: Keep Red (preparing to resume)
    LOG_INFO(Logger::TAG_SC, "Resuming traffic - Phase 1: car=RED (preparing)");
    driveCar(CAR, "Red");
    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
    
    // GREEN and SUCCESS event will be published by update() after 2 seconds
}

void SignalControl::halt() {
    // Emergency: force all signals into a safe state (RED for car and boat).
    ensurePins();
    LOG_WARN(Logger::TAG_SC, "EMERGENCY HALT - all signals RED");
    allSafeRed();
    LOG_WARN(Logger::TAG_SC, "All signals set to safe state");
}

void SignalControl::resetToIdleState() {
    ensurePins();

    LOG_INFO(Logger::TAG_SC, "Resetting signals to idle defaults (car=GREEN, boats=RED)");

    m_currentOperation = Operation::NONE;
    m_stopPhase = StopPhase::COMPLETE;
    m_resumePhase = ResumePhase::COMPLETE;
    m_operationStartTime = 0;

    m_boatQueueActive = false;
    m_boatQueueStartTime = 0;
    m_boatQueueSide = "";

    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
    driveCar(CAR, "Green");

    LOG_INFO(Logger::TAG_SC, "Signals reset complete");
}

void SignalControl::update() {
    // Check boat queue timer first
    if (m_boatQueueActive) {
        unsigned long queueElapsed = millis() - m_boatQueueStartTime;
        if (queueElapsed >= BOAT_GREEN_PERIOD_MS) {
            LOG_INFO(Logger::TAG_SC, "Boat green period expired (45s) - turning lights RED");
            endBoatGreenPeriod();
            
            // Publish event to notify state machine
            auto* expiredData = new SimpleEventData(BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED);
            m_eventBus.publish(BridgeEvent::BOAT_GREEN_PERIOD_EXPIRED, expiredData);
        }
    }
    
    if (m_currentOperation == Operation::NONE) {
        return;  // No operation in progress
    }
    
    unsigned long elapsed = millis() - m_operationStartTime;
    
    if (m_currentOperation == Operation::STOPPING_TRAFFIC) {
        switch (m_stopPhase) {
            case StopPhase::YELLOW_WARNING:
                if (elapsed >= 4000) {  
                    LOG_INFO(Logger::TAG_SC, "Stopping traffic - Phase 2: car=RED (clearance)");
                    driveCar(CAR, "Red");
                    m_stopPhase = StopPhase::RED_CLEARANCE;
                }
                break;
                
            case StopPhase::RED_CLEARANCE:
                if (elapsed >= 6000) {  
                    LOG_INFO(Logger::TAG_SC, "Traffic stopped successfully");
                    auto* stoppedData = new SimpleEventData(BridgeEvent::TRAFFIC_STOPPED_SUCCESS);
                    m_eventBus.publish(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, stoppedData);
                    m_stopPhase = StopPhase::COMPLETE;
                    m_currentOperation = Operation::NONE;
                }
                break;
                
            case StopPhase::COMPLETE:
                // Already complete
                break;
        }
    }
    else if (m_currentOperation == Operation::RESUMING_TRAFFIC) {
        switch (m_resumePhase) {
            case ResumePhase::RED_WAITING:
                if (elapsed >= 2000) {  
                    LOG_INFO(Logger::TAG_SC, "Resuming traffic - Phase 2: car=GREEN");
                    driveCar(CAR, "Green");
                    
                    LOG_INFO(Logger::TAG_SC, "Traffic resumed successfully");
                    auto* resumedData = new SimpleEventData(BridgeEvent::TRAFFIC_RESUMED_SUCCESS);
                    m_eventBus.publish(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, resumedData);
                    
                    m_resumePhase = ResumePhase::GREEN_GO;
                    m_currentOperation = Operation::NONE;
                }
                break;
                
            case ResumePhase::GREEN_GO:
            case ResumePhase::COMPLETE:
                // Already complete
                break;
        }
    }
}

void SignalControl::setCarTraffic(const String& color) {
    // Set both car lights to the same colour
    ensurePins();
    LOG_INFO(Logger::TAG_SC, "setCarTraffic(%s) - setting car group", color.c_str());
    
    // Apply to single (mirrored) car light group
    driveCar(CAR,  color);
    
    LOG_INFO(Logger::TAG_SC, "Car traffic updated successfully");
    
    // Publish single success event for car traffic (not separate left/right)
    auto* carTrafficData = new LightChangeData("both", color, true);
    m_eventBus.publish(BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS, carTrafficData);
}

void SignalControl::setBoatLight(const String& side, const String& color) {
    // TODO: Implement actual hardware control
    // Control the specific boat traffic light

    // Low-level command: set a specific boat side ("left"/"right") to Red/Green.
    ensurePins();
    LOG_INFO(Logger::TAG_SC, "setBoatLight(%s, %s)", side.c_str(), color.c_str());
    if (lower(side)=="left")  driveBoat(BOAT_LEFT,  color);
    if (lower(side)=="right") driveBoat(BOAT_RIGHT, color);
    // For now, just log the change
    LOG_INFO(Logger::TAG_SC, "Boat light updated successfully");
    
    // Publish success event with light change data
    auto* lightData = new LightChangeData(side, color, false);  // false = boat light
    m_eventBus.publish(BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS, lightData);
}

void SignalControl::startBoatGreenPeriod(const String& side) {
    ensurePins();
    LOG_INFO(Logger::TAG_SC, "Starting boat green period for %s side (45 seconds)", side.c_str());
    
    if (m_boatQueueActive) {
        LOG_INFO(Logger::TAG_SC, "Existing boat green period active on %s - restarting for %s",
                 m_boatQueueSide.c_str(), side.c_str());
        endBoatGreenPeriod();
    }

    // Set the specified side to green, other side to red
    if (lower(side) == "left") {
        driveBoat(BOAT_LEFT, "Green");
        driveBoat(BOAT_RIGHT, "Red");
    } else if (lower(side) == "right") {
        driveBoat(BOAT_RIGHT, "Green");
        driveBoat(BOAT_LEFT, "Red");
    }
    
    // Start queue timer
    m_boatQueueActive = true;
    m_boatQueueStartTime = millis();
    m_boatQueueSide = side;
    
    LOG_INFO(Logger::TAG_SC, "Boat queue timer started - boats can pass for 45 seconds");
}

void SignalControl::endBoatGreenPeriod() {
    if (!m_boatQueueActive) {
        LOG_DEBUG(Logger::TAG_SC, "endBoatGreenPeriod() called but no active timer");
        return;  // Already inactive
    }
    
    ensurePins();
    LOG_INFO(Logger::TAG_SC, "Ending boat green period - setting both sides to RED");
    
    // Set both boat lights to red
    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
    
    // Clear queue state
    m_boatQueueActive = false;
    m_boatQueueStartTime = 0;
    m_boatQueueSide = "";
    
    LOG_INFO(Logger::TAG_SC, "Boat queue timer ended - waiting for boat passage confirmation");
}

bool SignalControl::isBoatGreenPeriodActive() const {
    return m_boatQueueActive;
}
