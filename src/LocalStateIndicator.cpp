#include "LocalStateIndicator.h"
#include "BridgeSystemDefs.h"
#include <Arduino.h>
namespace {
  constexpr bool ACTIVE_HIGH = true;
  constexpr uint8_t LED_R = 21;
  constexpr uint8_t LED_Y = 19;
  constexpr uint8_t LED_G = 18;

  bool pinsReady = false;

  enum class Colour { Red, Yellow, Green };
  volatile Colour currentColour = Colour::Green;
  volatile bool inFault = false;
  volatile bool manual  = false;

  inline void writePin(uint8_t pin, bool on){
    digitalWrite(pin, (on == ACTIVE_HIGH) ? HIGH : LOW);
  }
  void ensurePins(){
    if (pinsReady) return;
    pinMode(LED_R, OUTPUT); pinMode(LED_Y, OUTPUT); pinMode(LED_G, OUTPUT);
    writePin(LED_R,false); writePin(LED_Y,false); writePin(LED_G,false);
    pinsReady = true;
  }
  void setLamp(Colour c){
    ensurePins();
    switch (c){
      case Colour::Red:    writePin(LED_R,true);  writePin(LED_Y,false); writePin(LED_G,false); break;
      case Colour::Yellow: writePin(LED_R,false); writePin(LED_Y,true);  writePin(LED_G,false); break;
      case Colour::Green:  writePin(LED_R,false); writePin(LED_Y,false); writePin(LED_G,true ); break;
    }
  }
}


LocalStateIndicator::LocalStateIndicator(EventBus& eventBus) : m_eventBus(eventBus) {
    Serial.println("LOCAL_STATE_INDICATOR: Initialised");
    using E = BridgeEvent;
    auto cb = [this](EventData* d){
    if (!d) return;
    const auto ev = d->getEventEnum();

    // Fault dominates
    if (ev == E::FAULT_DETECTED || ev == E::SYSTEM_SAFE_SUCCESS) {
      inFault = true; currentColour = Colour::Red; this->setState(); return;
    }
    if (ev == E::FAULT_CLEARED) { inFault = false; }

    // Manual override hint
    if (ev == E::MANUAL_OVERRIDE_ACTIVATED)   manual = true;
    if (ev == E::MANUAL_OVERRIDE_DEACTIVATED) manual = false;

    if (!inFault) {
      // Green when traffic resumed/cleared; Yellow during stopped/open/close or manual
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
    setLamp(inFault ? Colour::Red : currentColour);
    m_eventBus.publish(BridgeEvent::INDICATOR_UPDATE_SUCCESS, nullptr);
    // Publish success event
    Serial.println("LOCAL_STATE_INDICATOR: State display updated successfully");
    m_eventBus.publish(BridgeEvent::INDICATOR_UPDATE_SUCCESS, nullptr);
}

void LocalStateIndicator::halt() {
    Serial.println("LOCAL_STATE_INDICATOR: EMERGENCY HALT - setting display to FAULT state");
    
    // TODO: Implement actual hardware control
    // Set status indicators to show fault/emergency state
    // Flash warning lights, show error messages, etc.
    inFault = true; currentColour = Colour::Red;
    setLamp(Colour::Red);
    Serial.println("LOCAL_STATE_INDICATOR: Display set to fault state");
}
