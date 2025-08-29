#pragma once
#include <WiFi.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "StateWriter.h"

class WebSocketServer {
public:
    WebSocketServer(uint16_t port, StateWriter& StateWriter);

    void beginWiFi(const char* ssid, const char* password);
    void startServer();
    void handleClients();
    void checkWiFiStatus();

private:
    StateWriter& state_;

    AsyncWebServer server;
    AsyncWebSocket ws;
    uint16_t port;
    unsigned long lastStatusCheck;

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
    void fillSystemStatus(JsonObject obj);
};
