#include <Arduino.h>
#include "BridgeStateMachine.h"
#include "Controller.h"
#include "EventBus.h"
#include "CommandBus.h"
#include "MotorControl.h"
#include "SignalControl.h"
#include "LocalStateIndicator.h"
#include "WebSocketServer.h"
#include "StateWriter.h"
#include "credentials.h"

#define LED_BUILTIN 2

// Buses
EventBus systemEventBus;
CommandBus systemCommandBus;

// Subsystems
MotorControl motorControl(systemEventBus);
SignalControl signalControl(systemEventBus);
LocalStateIndicator localStateIndicator(systemEventBus);

// Main components
Controller controller(systemEventBus, systemCommandBus, motorControl, 
                     signalControl, localStateIndicator);
BridgeStateMachine stateMachine(systemEventBus, systemCommandBus);

// WebSocket and state monitoring components
StateWriter stateWriter;
WebSocketServer wss(80, stateWriter);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("======= ESP32 Bridge Control System Starting =======");
    
    Serial.println("Initialising EventBus and CommandBus..."); // Setup above
    Serial.println("Initialising subsystems..."); // All subsystems handle initialisation internally
    
    Serial.println("Initialising Controller...");
    controller.begin();
    
    Serial.println("Initialising State Machine...");
    stateMachine.begin();
    wss.beginWiFi(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Beginning state writer subscriptions...");
    stateWriter.beginSubscriptions();
    
    Serial.println("Starting WebSocket server...");
    wss.startServer(); 
    
    // builtin LED for crude heartbeat 
    pinMode(LED_BUILTIN, OUTPUT);
    
    Serial.println("=== Bridge Control System Ready ===");
    Serial.println("State Machine: " + stateMachine.getStateString());
    Serial.println("Waiting for events...");
    Serial.println("=======================================");
}

void loop() {
    // Process all pending events
    systemEventBus.processEvents();
    
    // Handle WebSocket communications
    wss.checkWiFiStatus();
    
    // Status LED heartbeat
    static unsigned long lastHeartbeat = 0;
    static bool ledState = false;
    
    if (millis() - lastHeartbeat > 2000) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
        lastHeartbeat = millis();
    }
    
    delay(10);
}