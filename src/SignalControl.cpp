#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include <Arduino.h>

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

SignalControl::SignalControl(EventBus& eventBus) : m_eventBus(eventBus) {
    Serial.println("SIGNAL_CONTROL: Initialised");
}

void SignalControl::begin() {
    ensurePins();
    // Default bridge state on boot: allow road traffic, hold boats.
    Serial.println("SIGNAL_CONTROL: Applying default signal state (car=GREEN; boats=RED)");
    driveCar(CAR, "Green");
    driveBoat(BOAT_LEFT, "Red");
    driveBoat(BOAT_RIGHT, "Red");
}

void SignalControl::stopTraffic() {
    // TODO: Implement actual hardware control
    // Set traffic lights to red
    // Control boat signals
    
    // High-level action: stop road traffic (car lights RED), keep boats RED (bridge closed).
    ensurePins();
    Serial.println("SIGNAL_CONTROL: Stopping traffic - car=RED; boats=RED");
    driveCar(CAR,  "Red");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
    // Publish success event

    // Notify FSM/UI that "StoppingTraffic" step completed.
    Serial.println("SIGNAL_CONTROL: Traffic stopped successfully");
    auto* stoppedData = new SimpleEventData(BridgeEvent::TRAFFIC_STOPPED_SUCCESS);
    m_eventBus.publish(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, stoppedData);
}

void SignalControl::resumeTraffic() {
    Serial.println("SIGNAL_CONTROL: Resuming traffic - setting signals to GREEN");
    
    // TODO:
    // Set traffic lights to green
    // Update boat signals 

    // High-level action: resume road traffic (car lights GREEN).
    // Typically keep boats RED when bridge is closed.
    ensurePins();
    Serial.println("SIGNAL_CONTROL: Resuming traffic - car=GREEN; boats=RED");
    driveCar(CAR,  "Green");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
    // Publish success event
    Serial.println("SIGNAL_CONTROL: Traffic resumed successfully");
    auto* resumedData = new SimpleEventData(BridgeEvent::TRAFFIC_RESUMED_SUCCESS);
    m_eventBus.publish(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, resumedData);
}

void SignalControl::halt() {
    // TODO: 
    // Set all signals to safe state (red for traffic, red for boats)

    // Emergency: force all signals into a safe state (RED for car and boat).
    ensurePins();
    Serial.println("SIGNAL_CONTROL: EMERGENCY HALT - all signals RED");
    allSafeRed();
    Serial.println("SIGNAL_CONTROL: All signals set to safe state");
}

void SignalControl::setCarTraffic(const String& color) {
    // Set both car lights to the same colour
    ensurePins();
    Serial.printf("SIGNAL_CONTROL: setCarTraffic(%s) - setting car group\n", color.c_str());
    
    // Apply to single (mirrored) car light group
    driveCar(CAR,  color);
    
    Serial.println("SIGNAL_CONTROL: Car traffic updated successfully");
    
    // Publish single success event for car traffic (not separate left/right)
    auto* carTrafficData = new LightChangeData("both", color, true);
    m_eventBus.publish(BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS, carTrafficData);
}

void SignalControl::setBoatLight(const String& side, const String& color) {
    // TODO: Implement actual hardware control
    // Control the specific boat traffic light

    // Low-level command: set a specific boat side ("left"/"right") to Red/Green.
    ensurePins();
    Serial.printf("SIGNAL_CONTROL: setBoatLight(%s, %s)\n", side.c_str(), color.c_str());
    if (lower(side)=="left")  driveBoat(BOAT_LEFT,  color);
    if (lower(side)=="right") driveBoat(BOAT_RIGHT, color);
    // For now, just log the change
    Serial.println("SIGNAL_CONTROL: Boat light updated successfully");
    
    // Publish success event with light change data
    auto* lightData = new LightChangeData(side, color, false);  // false = boat light
    m_eventBus.publish(BridgeEvent::BOAT_LIGHT_CHANGED_SUCCESS, lightData);
}
