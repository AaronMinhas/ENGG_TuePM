#include "WebSocketServer.h"

WebSocketServer::WebSocketServer(uint16_t port) : server(port), ws("/ws"), port(port), lastStatusCheck(0) {}

void WebSocketServer::beginWiFi(const char* ssid, const char* password) {
    Serial.print("Connecting to WiFi network: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void WebSocketServer::startServer() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        handleWsEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "ESP32 WebSocket Server Running");
    });

    server.begin();
    Serial.println("WebSocket server started successfully!");
    Serial.print("Connect to: ws://");
    Serial.print(WiFi.localIP());
    Serial.println("/ws");
}

void WebSocketServer::handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
    
    if (type == WS_EVT_CONNECT) {
        Serial.printf("Client %u connected\n", client->id());
        client->text("{\"status\":\"connected\"}");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("Client %u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        String msg = "";
        for (size_t i = 0; i < len; i++) {
            msg += (char)data[i];
        }
        Serial.printf("Received from client %u: %s\n", client->id(), msg.c_str());

        if (msg == "OPEN") {
            client->text("{\"bridge\":\"opening\"}");
        } else if (msg == "CLOSE") {
            client->text("{\"bridge\":\"closing\"}");
        } else {
            client->text("{\"echo\":\"" + msg + "\"}");
        }
    }
}

// void SocketServer::handleClients() {
//     WiFiClient client = server.available();
//     if (client) {
//         Serial.println("New client connected!");
//         Serial.print("Client IP: ");
//         Serial.println(client.remoteIP());
        
//         while (client.connected()) {
//             if (client.available()) {
//                 String msg = client.readStringUntil('\n');
//                 Serial.print("Received message: ");
//                 Serial.println(msg);
                
//                 String response = "{\"status\":\"acknowledged\"}";
//                 client.println(response);
//                 Serial.print("Sent response: ");
//                 Serial.println(response);
//             }
//         }
//         client.stop();
//         Serial.println("Client disconnected.");
//     }
// }

void WebSocketServer::checkWiFiStatus() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastStatusCheck > 30000) {
        lastStatusCheck = currentTime;
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WARNING: WiFi connection lost!");
            Serial.print("Current status: ");
            switch (WiFi.status()) {
                case WL_IDLE_STATUS:
                    Serial.println("Idle");
                    break;
                case WL_NO_SSID_AVAIL:
                    Serial.println("Network not available");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("Connection failed");
                    break;
                case WL_CONNECTION_LOST:
                    Serial.println("Connection lost");
                    break;
                case WL_DISCONNECTED:
                    Serial.println("Disconnected");
                    break;
                default:
                    Serial.println("Unknown");
            }
            Serial.println("Attempting to reconnect ->");
            WiFi.reconnect();
        } else {
            Serial.println("WiFi Status: Connected");
        }
    }
}