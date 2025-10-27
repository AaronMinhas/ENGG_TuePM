#pragma once
#include <WiFi.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "StateWriter.h"
#include "CommandBus.h"
#include "EventBus.h"

class ConsoleCommands;

class WebSocketServer {
public:
    WebSocketServer(uint16_t port, StateWriter& StateWriter, CommandBus& commandBus, EventBus& eventBus);

    void configureWiFi(const char* ssid, const char* password);
    void networkLoop();
    void attachConsole(ConsoleCommands* console);

private:
    StateWriter& state_;
    CommandBus& commandBus_;
    EventBus& eventBus_;
    ConsoleCommands* console_ = nullptr;

    AsyncWebServer server;
    AsyncWebSocket ws;
    uint16_t port;
    bool wifiConfigured_ = false;
    bool serverStarted_ = false;
    bool handlersAttached_ = false;
    bool broadcastSubscribed_ = false;
    bool connectionInProgress_ = false;
    unsigned long connectStartMs_ = 0;
    unsigned long nextRetryMs_ = 0;
    String ssid_;
    String password_;

    String bridgeState = "Closed";
    bool lockEngaged = true;
    uint32_t bridgeLastChangeMs = 0;

    // These need to grab the state from the StateWriter --> e.g. carTrafficLeft = StateWriter.carTrafficLeft.RED
    String carTrafficLeft, carTrafficRight;
    String boatTrafficLeft, boatTrafficRight;

    void handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                       AwsEventType type, void* arg, uint8_t* data, size_t len);

    void handleGet(AsyncWebSocketClient* client, const String& id, const String& path);
    
    void handleSet(AsyncWebSocketClient* client, const String& id, const String& path, 
                   JsonVariant payload);

    void sendOk(AsyncWebSocketClient* client, const String& id, const String& path,
                std::function<void(JsonObject)> fillPayload = nullptr);
    
    void sendError(AsyncWebSocketClient* client, const String& id, const String& path, const String& msg);

    void fillBridgeStatus(JsonObject obj);
    void fillCarTrafficStatus(JsonObject obj);
    void fillBoatTrafficStatus(JsonObject obj);
    void fillVehicleTrafficStatus(JsonObject obj);
    void fillSystemStatus(JsonObject obj);

    void broadcastSnapshot();
    void setupBroadcastSubscriptions();
    void startServer();
};
