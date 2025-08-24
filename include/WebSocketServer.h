#pragma once
#include <WiFi.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>

class WebSocketServer {
public:
    WebSocketServer(uint16_t port);
    void beginWiFi(const char* ssid, const char* password);
    void startServer();
    void handleClients();
    void checkWiFiStatus();
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    uint16_t port;
    unsigned long lastStatusCheck;

    void handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                       AwsEventType type, void* arg, uint8_t* data, size_t len);
};
