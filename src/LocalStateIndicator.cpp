#include "LocalStateIndicator.h"
#include "BridgeSystemDefs.h"
#include <Arduino.h>

/*
  Module: LocalStateIndicator
  Purpose:
    - Show the overall system condition on a local tri-LED (R/Y/G).
    - Subscribe to key events (traffic stopped/resumed, bridge opened/closed, faults, manual mode).
    - Pick a colour policy:
        * Red    = fault/safe (dominates)
        * Yellow = in-between / action ongoing (stopped/opened/closed) or manual override active
        * Green  = normal traffic resumed / fault cleared
    - Publish INDICATOR_UPDATE_SUCCESS after each refresh so StateWriter/UI can log/sync.
*/
namespace {
  // LED polarity: set to false if hardware is active-low (LOW = ON).
  constexpr bool ACTIVE_HIGH = true;

  // GPIO pins for the local tri-LED.
  constexpr uint8_t LED_R = 21;
  constexpr uint8_t LED_Y = 19;
  constexpr uint8_t LED_G = 18;

  // One-time pin setup flag.
  bool pinsReady = false;

  // Local colour state for the indicator.
  enum class Colour { Red, Yellow, Green };
  volatile Colour currentColour = Colour::Green;
  volatile bool inFault = false;
  volatile bool manual  = false;

  // Drive one pin ON/OFF honoring ACTIVE_HIGH policy.
  inline void writePin(uint8_t pin, bool on){
    digitalWrite(pin, (on == ACTIVE_HIGH) ? HIGH : LOW);
  }

  // Ensure pins are configured and driven to a safe default (all OFF).
  void ensurePins(){
    if (pinsReady) return;
    pinMode(LED_R, OUTPUT); pinMode(LED_Y, OUTPUT); pinMode(LED_G, OUTPUT);
    writePin(LED_R,false); writePin(LED_Y,false); writePin(LED_G,false);
    pinsReady = true;
  }

  // Set the tri-LED to a specific colour (mutually exclusive).
  void setLamp(Colour c){
    ensurePins();
    switch (c){
      case Colour::Red:    writePin(LED_R,true);  writePin(LED_Y,false); writePin(LED_G,false); break;
      case Colour::Yellow: writePin(LED_R,false); writePin(LED_Y,true);  writePin(LED_G,false); break;
      case Colour::Green:  writePin(LED_R,false); writePin(LED_Y,false); writePin(LED_G,true ); break;
    }
  }
}
// gggg


LocalStateIndicator::LocalStateIndicator(EventBus& eventBus) : m_eventBus(eventBus) {
    Serial.println("LOCAL_STATE_INDICATOR: Initialised");
    using E = BridgeEvent;
    // Unified callback for all subscribed events; adjusts local flags/colour policy and refreshes LED.
    auto cb = [this](EventData* d){
    if (!d) return;
    const auto ev = d->getEventEnum();

    // Fault dominates everything (including SYSTEM_SAFE_SUCCESS coming from controller halt).
    if (ev == E::FAULT_DETECTED || ev == E::SYSTEM_SAFE_SUCCESS) {
      inFault = true; currentColour = Colour::Red; this->setState(); return;
    }
    if (ev == E::FAULT_CLEARED) { inFault = false; }

    // Manual override hint
    if (ev == E::MANUAL_OVERRIDE_ACTIVATED)   manual = true;
    if (ev == E::MANUAL_OVERRIDE_DEACTIVATED) manual = false;

    // Colour selection when not in fault:
    //  - Green  when traffic is normal (resumed/cleared)
    //  - Yellow when traffic is stopped, bridge just opened/closed, or manual override is on
    if (!inFault) {
      if (ev == E::TRAFFIC_RESUMED_SUCCESS || ev == E::FAULT_CLEARED) {
        currentColour = Colour::Green;
      } else if (ev == E::TRAFFIC_STOPPED_SUCCESS ||
                 ev == E::BRIDGE_OPENED_SUCCESS   ||
                 ev == E::BRIDGE_CLOSED_SUCCESS   ||
                 manual) {
        currentColour = Colour::Yellow;
      }
    }
    this->setState();
  };

  // Subscribe to the key events that drive the indicator policy.
  m_eventBus.subscribe(E::TRAFFIC_STOPPED_SUCCESS, cb);
  m_eventBus.subscribe(E::BRIDGE_OPENED_SUCCESS,   cb);
  m_eventBus.subscribe(E::BRIDGE_CLOSED_SUCCESS,   cb);
  m_eventBus.subscribe(E::TRAFFIC_RESUMED_SUCCESS, cb);
  m_eventBus.subscribe(E::FAULT_DETECTED,          cb);
  m_eventBus.subscribe(E::FAULT_CLEARED,           cb);
  m_eventBus.subscribe(E::SYSTEM_SAFE_SUCCESS,     cb);
  m_eventBus.subscribe(E::MANUAL_OVERRIDE_ACTIVATED,   cb);
  m_eventBus.subscribe(E::MANUAL_OVERRIDE_DEACTIVATED, cb);
}


void LocalStateIndicator::setState() {
    Serial.println("LOCAL_STATE_INDICATOR: Updating state display");
    
    // TODO: Implement actual hardware control
    // Update LED status indicators, displays, etc.
    // Show current bridge state (idle, opening, open, closing, etc.)

    // Drive the physical LED according to policy; Red dominates if inFault==true.
    setLamp(inFault ? Colour::Red : currentColour);
    // Notify that the indicator was refreshed.
    auto* indicatorData = new SimpleEventData(BridgeEvent::INDICATOR_UPDATE_SUCCESS);
    m_eventBus.publish(BridgeEvent::INDICATOR_UPDATE_SUCCESS, indicatorData);
    Serial.println("LOCAL_STATE_INDICATOR: State display updated successfully");
}

void LocalStateIndicator::halt() {
    Serial.println("LOCAL_STATE_INDICATOR: EMERGENCY HALT - setting display to FAULT state");
    
    // TODO: Implement actual hardware control
    // Set status indicators to show fault/emergency state
    // Flash warning lights, show error messages, etc.

    // Force local fault state and show Red immediately.
    inFault = true; currentColour = Colour::Red;
    setLamp(Colour::Red);
    Serial.println("LOCAL_STATE_INDICATOR: Display set to fault state");
}
