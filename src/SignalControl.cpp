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

  // Car traffic lights: each side has Red/Yellow/Green pins.
  struct RGB { uint8_t r, y, g; };
  // Boat lights: each side has Red/Green pins.
  struct RG  { uint8_t r, g;  };

  RGB CAR_LEFT  {14, 27, 26};
  RGB CAR_RIGHT {25, 33, 32};

  // Boat lights (R/G) per side
  RG  BOAT_LEFT  {4, 16};
  RG  BOAT_RIGHT {17, 5};

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
    // Car
    prep(CAR_LEFT.r);  prep(CAR_LEFT.y);  prep(CAR_LEFT.g);
    prep(CAR_RIGHT.r); prep(CAR_RIGHT.y); prep(CAR_RIGHT.g);
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
    driveCar(CAR_LEFT,  "Red");
    driveCar(CAR_RIGHT, "Red");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
  }
} 

SignalControl::SignalControl(EventBus& eventBus) : m_eventBus(eventBus) {
    Serial.println("SIGNAL_CONTROL: Initialised");
}

void SignalControl::stopTraffic() {
    // TODO: Implement actual hardware control
    // Set traffic lights to red
    // Control boat signals
    
    // High-level action: stop road traffic (car lights RED), keep boats RED (bridge closed).
    ensurePins();
    Serial.println("SIGNAL_CONTROL: Stopping traffic - car L/R=RED; boats=RED");
    driveCar(CAR_LEFT,  "Red");
    driveCar(CAR_RIGHT, "Red");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
    // Publish success event

    // Notify FSM/UI that "StoppingTraffic" step completed.
    Serial.println("SIGNAL_CONTROL: Traffic stopped successfully");
    m_eventBus.publish(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, nullptr);
}

void SignalControl::resumeTraffic() {
    Serial.println("SIGNAL_CONTROL: Resuming traffic - setting signals to GREEN");
    
    // TODO:
    // Set traffic lights to green
    // Update boat signals 

    // High-level action: resume road traffic (car lights GREEN).
    // Typically keep boats RED when bridge is closed.
    ensurePins();
    Serial.println("SIGNAL_CONTROL: Resuming traffic - car L/R=GREEN; boats=RED");
    driveCar(CAR_LEFT,  "Green");
    driveCar(CAR_RIGHT, "Green");
    driveBoat(BOAT_LEFT,  "Red");
    driveBoat(BOAT_RIGHT, "Red");
    // Publish success event
    Serial.println("SIGNAL_CONTROL: Traffic resumed successfully");
    m_eventBus.publish(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, nullptr);
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

void SignalControl::setCarLight(const String& side, const String& color) {
    // TODO: Implement actual hardware control
    // Control the specific car traffic light

    // Low-level command: set a specific car side ("left"/"right") to Red/Yellow/Green.
    ensurePins();
    Serial.printf("SIGNAL_CONTROL: setCarLight(%s, %s)\n", side.c_str(), color.c_str());
    // Apply per side (case-insensitive); invalid side is ignored (nothing driven).
    if (lower(side)=="left")  driveCar(CAR_LEFT,  color);
    if (lower(side)=="right") driveCar(CAR_RIGHT, color);
    // For now, just log the change
    Serial.println("SIGNAL_CONTROL: Car light updated successfully");
    
    // Publish success event with light change data
    auto* lightData = new LightChangeData(side, color, true);  // true = car light
    m_eventBus.publish(BridgeEvent::CAR_LIGHT_CHANGED_SUCCESS, lightData);
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
