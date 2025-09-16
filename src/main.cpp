#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "BridgeStateMachine.h"
#include "Controller.h"
#include "EventBus.h"
#include "CommandBus.h"
#include "MotorControl.h"
#include "SignalControl.h"
#include "LocalStateIndicator.h"
#include "WebSocketServer.h"
#include "StateWriter.h"
#include "DetectionSystem.h"
#include "ConsoleCommands.h"
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
StateWriter stateWriter(systemEventBus);
WebSocketServer wss(80, stateWriter, systemCommandBus, systemEventBus);

// Sensors
DetectionSystem detectionSystem(systemEventBus);

// Console router
ConsoleCommands console(motorControl, detectionSystem);

// Task handles for FreeRTOS
TaskHandle_t controlLogicTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// Core assignments
#define CONTROL_LOGIC_CORE 1  // High priority core for bridge control
#define NETWORK_CORE 0        // Lower priority core for networking

// CONTROL LOGIC CORE TASK (High Priority - Core 1)
void controlLogicTask(void* parameters) {
    Serial.println("CONTROL_LOGIC_CORE: Task started on Core 1");
    
    // Status LED heartbeat variables
    unsigned long lastHeartbeat = 0;
    bool ledState = false;
    
    while (true) {
        systemEventBus.processEvents();
        
        // Check console commands
        console.poll();
        
        // Check motor operation progress (non-blocking)
        motorControl.checkProgress();
        
        // Monitor sensors (ultrasonic distance → events)
        detectionSystem.update();
        
        // TODO: Monitor system health  
        // safetyManager.checkSystemHealth();
        
        // LED heartbeat
        // if (millis() - lastHeartbeat > 2000) {
        //     ledState = !ledState;
        //     digitalWrite(LED_BUILTIN, ledState);
        //     lastHeartbeat = millis();
        // }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// NETWORK CORE TASK (Lower Priority - Core 0)  
void networkTask(void* parameters) {
    Serial.println("NETWORK_CORE: Task started on Core 0");
    
    while (true) {
        // Checks WiFi is still connected. Triggers the following:
        // - WebSocket message processing ( callbacks when messages arrive)
        // - Frontend dashboard updates (StateWriter updates via EventBus automatically) 
        // - Status API responses (handled automatically when frontend requests data)
        wss.checkWiFiStatus();
        

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("======= ESP32 Bridge Control System Starting =======");
    Serial.print("Total heap: "); Serial.println(ESP.getHeapSize());
    Serial.print("Free heap: "); Serial.println(ESP.getFreeHeap());
    Serial.print("Total PSRAM: "); Serial.println(ESP.getPsramSize());
    Serial.print("Free PSRAM: "); Serial.println(ESP.getFreePsram());
    
    pinMode(LED_BUILTIN, OUTPUT);
    
    Serial.println("Initialising EventBus and CommandBus..."); // Setup above
    Serial.println("Initialising subsystems..."); // All subsystems handle initialisation internally
    
    Serial.println("Initialising Motor Control...");
    motorControl.init();
    
    Serial.println("Initialising Controller...");
    controller.begin();
    
    Serial.println("Initialising State Machine...");
    stateMachine.begin();
    wss.beginWiFi(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Beginning state writer subscriptions...");
    stateWriter.beginSubscriptions();

    Serial.println("Initialising Detection System (ultrasonic)...");
    detectionSystem.begin();
    Serial.println("Detection System ready for bi-directional boat tracking");
    
    Serial.println("Starting WebSocket server...");
    wss.startServer(); 
    
    // Initialise console after Serial is ready
    console.begin();
    
    Serial.println("=== Bridge Control System Ready ===");
    Serial.println("State Machine: " + stateMachine.getStateString());
    
    Serial.println("Creating Control Logic Core task (Core 1)...");
    xTaskCreatePinnedToCore(
        controlLogicTask,           // Task function
        "ControlLogicTask",         // Task name
        4096,                       // Stack size (bytes)
        NULL,                       // Task parameters
        configMAX_PRIORITIES - 1,   // Priority (highest)
        &controlLogicTaskHandle,    // Task handle
        CONTROL_LOGIC_CORE          // CPU core (1)
    );
    
    Serial.println("Creating Network Core task (Core 0)...");
    xTaskCreatePinnedToCore(
        networkTask,                // Task function
        "NetworkTask",              // Task name  
        8192,                       // Stack size (bytes)
        NULL,                       // Task parameters
        1,                          // Priority (lower than control logic)
        &networkTaskHandle,         // Task handle
        NETWORK_CORE                // CPU core (0)
    );
    
    if (controlLogicTaskHandle != NULL && networkTaskHandle != NULL) {
        Serial.println("Dual core tasks created successfully.");
        Serial.println("Control Logic Core: Sensors, State Machine, Safety");
        Serial.println("Network Core: WebSocket, WiFi, Remote UI");
        Serial.println("=======================================");
    } else {
        Serial.println("ERROR: Failed to create dual-core tasks.");
        Serial.println("System will not function properly.");
    }
}

void loop() {
    // Monitor task health
    if (controlLogicTaskHandle != NULL) {
    } else {
        Serial.println("WARNING: Control Logic task has stopped!");
    }
    
    if (networkTaskHandle != NULL) {
    } else {
        Serial.println("WARNING: Network task has stopped!");
    }
    
    delay(1000); // Check task health every second
}