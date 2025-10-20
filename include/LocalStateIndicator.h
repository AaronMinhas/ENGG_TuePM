#pragma once

#include "BridgeSystemDefs.h"
#include "EventBus.h"
#include <FastLED.h>

class LocalStateIndicator {
public:
    LocalStateIndicator(EventBus& eventBus);
    
    void begin();
    void setState();
    void update();  // Call from main loop for blinking
    void halt();
    
private:
    EventBus& m_eventBus;
    
    // LED configuration
    static constexpr uint8_t LED_PIN = 22;  // GPIO pin for GlowBit Stick
    static constexpr uint8_t NUM_LEDS = 8;   // GlowBit Stick 1x8
    static constexpr uint8_t BRIGHTNESS = 80; // Brightness level (0-255)
    
    CRGB leds[NUM_LEDS];
    BridgeState currentState;
    bool isBlinking;
    unsigned long lastBlinkTime;
    bool blinkState;  // true = on, false = off
    
    void setSolidColor(CRGB color);
    void setBlinkingColor(CRGB color);
    void rainbowStartup();
    void updateBlinking();
    CRGB getStateColor(BridgeState state);
    bool shouldBlink(BridgeState state);
};
