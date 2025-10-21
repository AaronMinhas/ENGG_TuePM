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
#include "SafetyManager.h"
#include "credentials.h"
#include "Logger.h"

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

// SafetyManager monitorint components
SafetyManager safetyManager(systemEventBus, systemCommandBus);

// Sensors
DetectionSystem detectionSystem(systemEventBus);

// Console router
ConsoleCommands console(motorControl, detectionSystem, systemEventBus, signalControl);

// Task handles for FreeRTOS
TaskHandle_t controlLogicTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// Core assignments
#define CONTROL_LOGIC_CORE 1  // High priority core for bridge control
#define NETWORK_CORE 0        // Lower priority core for networking

// CONTROL LOGIC CORE TASK (High Priority - Core 1)
void controlLogicTask(void* parameters) {
    LOG_INFO(Logger::TAG_SYS, "CONTROL_LOGIC_CORE: Task started on Core 1");
    
    // Status LED heartbeat variables
    unsigned long lastHeartbeat = 0;
    bool ledState = false;
    
    while (true) {
        systemEventBus.processEvents();
        
        // Check console commands
        console.poll();
        
        // Check motor operation progress (non-blocking)
        motorControl.checkProgress();
        
        // Process signal control timing (non-blocking traffic light transitions)
        signalControl.update();
        
        // Monitor sensors (ultrasonic distance → events)
        detectionSystem.update();
        
        // Update LED indicator (handles blinking)
        localStateIndicator.update();
        
        // Check state machine timeouts (boat passage timeout)
        stateMachine.checkTimeouts();
        
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
    LOG_INFO(Logger::TAG_SYS, "NETWORK_CORE: Task started on Core 0");
    
    while (true) {
        // Periodically attempt WiFi connection
        wss.networkLoop();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Logger::begin(Logger::Level::INFO);
    LOG_INFO(Logger::TAG_SYS, "======= ESP32 Bridge Control System Starting =======");
    LOG_INFO(Logger::TAG_SYS, "Total heap: %u", ESP.getHeapSize());
    LOG_INFO(Logger::TAG_SYS, "Free heap: %u", ESP.getFreeHeap());
    LOG_INFO(Logger::TAG_SYS, "Total PSRAM: %u", ESP.getPsramSize());
    LOG_INFO(Logger::TAG_SYS, "Free PSRAM: %u", ESP.getFreePsram());
    
    pinMode(LED_BUILTIN, OUTPUT);
    
    LOG_INFO(Logger::TAG_SYS, "Initialising EventBus and CommandBus...");
    LOG_INFO(Logger::TAG_SYS, "Initialising subsystems...");

    LOG_INFO(Logger::TAG_SYS, "Initializing Safety Manager...");
    safetyManager.setMotorControl(&motorControl);
    safetyManager.setSignalControl(&signalControl);
    safetyManager.begin();
    
    LOG_INFO(Logger::TAG_MC, "Initialising Motor Control...");
    motorControl.init();

    LOG_INFO(Logger::TAG_SC, "Initialising Signal Control outputs...");
    signalControl.begin();

    LOG_INFO(Logger::TAG_CMD, "Initialising Controller...");
    controller.begin();
    
    LOG_INFO(Logger::TAG_FSM, "Initialising State Machine...");
    stateMachine.begin();
    
    LOG_INFO(Logger::TAG_WS, "Configuring network services...");
    wss.configureWiFi(WIFI_SSID, WIFI_PASSWORD);

    LOG_INFO(Logger::TAG_EVT, "Beginning state writer subscriptions...");
    stateWriter.beginSubscriptions();

    LOG_INFO(Logger::TAG_DS, "Initialising Detection System (ultrasonic)...");
    detectionSystem.begin();
    LOG_INFO(Logger::TAG_DS, "Detection System ready for bi-directional boat tracking");

    LOG_INFO(Logger::TAG_LOC, "Initialising Local State Indicator...");
    localStateIndicator.begin();
    LOG_INFO(Logger::TAG_LOC, "GlowBit Stick indicator ready");

    LOG_INFO(Logger::TAG_WS, "Network services will start once WiFi is connected.");
    
    // Initialise console after Serial is ready
    console.begin();
    wss.attachConsole(&console);
    
    LOG_INFO(Logger::TAG_SYS, "=== Bridge Control System Ready ===");
    LOG_INFO(Logger::TAG_SYS, "State Machine: %s", stateMachine.getStateString().c_str());
    
    LOG_INFO(Logger::TAG_SYS, "Creating Control Logic Core task (Core 1)...");
    xTaskCreatePinnedToCore(
        controlLogicTask,           // Task function
        "ControlLogicTask",         // Task name
        4096,                       // Stack size (bytes)
        NULL,                       // Task parameters
        configMAX_PRIORITIES - 1,   // Priority (highest)
        &controlLogicTaskHandle,    // Task handle
        CONTROL_LOGIC_CORE          // CPU core (1)
    );
    
    LOG_INFO(Logger::TAG_SYS, "Creating Network Core task (Core 0)...");
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
        LOG_INFO(Logger::TAG_SYS, "Dual core tasks created successfully.");
        LOG_INFO(Logger::TAG_SYS, "Control Logic Core: Sensors, State Machine, Safety");
        LOG_INFO(Logger::TAG_SYS, "Network Core: WebSocket, WiFi, Remote UI");
        LOG_INFO(Logger::TAG_SYS, "=======================================");
    } else {
        LOG_ERROR(Logger::TAG_SYS, "Failed to create dual-core tasks.");
        LOG_ERROR(Logger::TAG_SYS, "System will not function properly.");
    }
}

void loop() {
    // Monitor task health
    safetyManager.update(); // SafetyManager watching
    if (controlLogicTaskHandle != NULL) {
    } else {
        LOG_WARN(Logger::TAG_SYS, "Control Logic task has stopped!");
    }
    
    if (networkTaskHandle != NULL) {
    } else {
        LOG_WARN(Logger::TAG_SYS, "Network task has stopped!");
    }
    
    delay(1000); // Check task health every second
}
