#include "LocalStateIndicator.h"
#include "BridgeSystemDefs.h"
#include "BridgeStateMachine.h"
#include <Arduino.h>
#include "Logger.h"
#include <FastLED.h>

/*
  Module: LocalStateIndicator
  Purpose:
    - Show the overall system condition on GlowBit Stick
    - Subscribe to STATE_CHANGED events and display bridge state with colour patterns.
    - Colour policy:
        * Green  = IDLE (ready)
        * Yellow = Traffic control (stopping/resuming) - blinking
        * Cyan   = Bridge motion (opening/closing) - blinking  
        * Blue   = Bridge open (waiting for boat) - solid
        * Purple = Manual mode - solid/blinking
        * Red    = Fault/Error - solid
    - Publish INDICATOR_UPDATE_SUCCESS after each refresh so StateWriter/UI can log and sync.
*/

LocalStateIndicator::LocalStateIndicator(EventBus& eventBus) 
    : m_eventBus(eventBus), currentState(BridgeState::IDLE), isBlinking(false), 
      lastBlinkTime(0), blinkState(false) {
    LOG_INFO(Logger::TAG_LOC, "Initialised GlowBit Stick 1x8 indicator");
    
    // Initialise FastLED
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    
    // Subscribe to STATE_CHANGED events
    auto stateCallback = [this](EventData* eventData) {
        if (!eventData) return;
        
        if (eventData->getEventEnum() == BridgeEvent::STATE_CHANGED) {
            StateChangeData* stateData = static_cast<StateChangeData*>(eventData);
            if (stateData) {
                currentState = stateData->getNewState();
                LOG_INFO(Logger::TAG_LOC, "State changed to: %s", 
                        BridgeStateMachine::stateName(currentState));
                setState();
            }
        }
    };
    
    m_eventBus.subscribe(BridgeEvent::STATE_CHANGED, stateCallback);
}

void LocalStateIndicator::begin() {
    LOG_INFO(Logger::TAG_LOC, "Starting GlowBit Stick indicator");
    
    // Clear all LEDs initially
    FastLED.clear();
    FastLED.show();
    
    // Run rainbow startup animation
    rainbowStartup();
    
    // Set initial state
    setState();
}

void LocalStateIndicator::setState() {
    LOG_DEBUG(Logger::TAG_LOC, "Updating LED display for state: %s", 
              BridgeStateMachine::stateName(currentState));
    
    CRGB color = getStateColor(currentState);
    isBlinking = shouldBlink(currentState);
    
    if (isBlinking) {
        setBlinkingColor(color);
    } else {
        setSolidColor(color);
    }
    
    // Publish success event
    auto* indicatorData = new SimpleEventData(BridgeEvent::INDICATOR_UPDATE_SUCCESS);
    m_eventBus.publish(BridgeEvent::INDICATOR_UPDATE_SUCCESS, indicatorData);
    LOG_DEBUG(Logger::TAG_LOC, "LED display updated successfully");
}

void LocalStateIndicator::update() {
    // Handle blinking for states that need it
    if (isBlinking) {
        updateBlinking();
    }
}

void LocalStateIndicator::halt() {
    LOG_WARN(Logger::TAG_LOC, "EMERGENCY HALT - setting display to FAULT state");
    
    // Force fault state
    currentState = BridgeState::FAULT;
    setSolidColor(CRGB::Red);
    LOG_WARN(Logger::TAG_LOC, "Display set to fault state");
}

void LocalStateIndicator::setSolidColor(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
    FastLED.show();
}

void LocalStateIndicator::setBlinkingColor(CRGB color) {
    // For blinking, we'll handle the on/off in updateBlinking()
    // This just sets the colour when it should be on
    if (blinkState) {
        setSolidColor(color);
    } else {
        FastLED.clear();
        FastLED.show();
    }
}

void LocalStateIndicator::updateBlinking() {
    unsigned long now = millis();
    if (now - lastBlinkTime >= 500) {  // 1 Hz blink (500ms on, 500ms off)
        blinkState = !blinkState;
        lastBlinkTime = now;
        
        CRGB color = getStateColor(currentState);
        if (blinkState) {
            setSolidColor(color);
        } else {
            FastLED.clear();
            FastLED.show();
        }
    }
}

void LocalStateIndicator::rainbowStartup() {
    LOG_INFO(Logger::TAG_LOC, "Running rainbow startup animation");
    
    // Rainbow cycle through all 8 LEDs
    for (int cycle = 0; cycle < 2; cycle++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            // Clear all LEDs
            FastLED.clear();
            
            // Set current LED to rainbow colour
            leds[i] = CHSV(i * 32, 255, 255);  // Hue varies by LED position
            FastLED.show();
            delay(100);
        }
    }
    
    // Final clear
    FastLED.clear();
    FastLED.show();
    delay(200);
    
    LOG_INFO(Logger::TAG_LOC, "Rainbow startup complete");
}

CRGB LocalStateIndicator::getStateColor(BridgeState state) {
    switch (state) {
        case BridgeState::IDLE:
            return CRGB::Green;
            
        case BridgeState::STOPPING_TRAFFIC:
        case BridgeState::RESUMING_TRAFFIC:
            return CRGB::Yellow;
            
        case BridgeState::OPENING:
        case BridgeState::CLOSING:
            return CRGB::Cyan;
            
        case BridgeState::OPEN:
            return CRGB::Blue;
            
        case BridgeState::FAULT:
            return CRGB::Red;
            
        case BridgeState::MANUAL_MODE:
        case BridgeState::MANUAL_OPEN:
        case BridgeState::MANUAL_CLOSED:
            return CRGB::Purple;
            
        case BridgeState::MANUAL_OPENING:
        case BridgeState::MANUAL_CLOSING:
            return CRGB::Purple;
            
        default:
            return CRGB::White;  // Unknown state
    }
}

bool LocalStateIndicator::shouldBlink(BridgeState state) {
    switch (state) {
        case BridgeState::STOPPING_TRAFFIC:
        case BridgeState::RESUMING_TRAFFIC:
        case BridgeState::OPENING:
        case BridgeState::CLOSING:
        case BridgeState::MANUAL_OPENING:
        case BridgeState::MANUAL_CLOSING:
            return true;
            
        default:
            return false;
    }
}
