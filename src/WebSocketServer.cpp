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

void WebSocketServer::fillBridgeStatus(JsonObject obj) {
    obj["state"] = bridgeState;
    obj["lastChangeMs"] = bridgeLastChangeMs;
    obj["lockEngaged"] = lockEngaged;
}

void WebSocketServer::fillTrafficStatus(JsonObject obj) {
    obj["left"]  = trafficLeft;
    obj["right"] = trafficRight;
}

void WebSocketServer::fillSystemStatus(JsonObject obj) {
    obj["connection"] = "CONNECTED";
    obj["rssi"] = WiFi.RSSI();
    obj["uptimeMs"] = millis();
}

void WebSocketServer::sendOk(AsyncWebSocketClient* client, const String& id, const String& path,
                             std::function<void(JsonObject)> fillPayload) {
    StaticJsonDocument<512> doc;
    doc["v"] = 1;
    doc["id"] = id;
    doc["type"] = "response";
    doc["ok"] = true;
    doc["path"] = path;
    if (fillPayload) {
        JsonObject payload = doc.createNestedObject("payload");
        fillPayload(payload);
    }
    String out; serializeJson(doc, out);
    client->text(out);

    Serial.printf("[WS][TX][OK] Client %u <- %s\n", client->id(), path.c_str());
    Serial.println(out);
}

void WebSocketServer::sendError(AsyncWebSocketClient* client, const String& id, const String& path, const String& msg) {
    StaticJsonDocument<384> doc;
    doc["v"] = 1;
    doc["id"] = id;
    doc["type"] = "response";
    doc["ok"] = false;
    doc["path"] = path;
    doc["error"] = msg;
    String out; serializeJson(doc, out);
    client->text(out);

    Serial.printf("[WS][TX][ERR] Client %u <- %s error=%s\n", client->id(), path.c_str(), msg.c_str());
}

void WebSocketServer::handleGet(AsyncWebSocketClient* client, const String& id, const String& path) {
    if (path == "/bridge/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillBridgeStatus(p); });
    } else if (path == "/traffic/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillTrafficStatus(p); });
    } else if (path == "/system/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillSystemStatus(p); });
    } else if (path == "/system/ping") {
        sendOk(client, id, path, [](JsonObject p){ p["nowMs"] = millis(); });
    } else {
        sendError(client, id, path, "Unknown GET path");
    }
}

void WebSocketServer::handleSet(AsyncWebSocketClient* client, const String& id, const String& path, JsonVariant payload) {
    if (path == "/bridge/state") {
        const char* state = payload["state"] | nullptr;
        if (!state) { sendError(client, id, path, "Missing 'state'"); return; }
        String s = String(state);
        if (s != "OPEN" && s != "CLOSED") { sendError(client, id, path, "Invalid state"); return; }

        bridgeState = s;
        lockEngaged = (s == "CLOSED");
        bridgeLastChangeMs = millis();

        sendOk(client, id, path, [this](JsonObject p){ fillBridgeStatus(p); });

    } else if (path == "/traffic/light") {
        const char* side = payload["side"] | nullptr;
        const char* value = payload["value"] | nullptr;
        if (!side || !value) { sendError(client, id, path, "Missing 'side' or 'value'"); return; }

        String sd = String(side), v = String(value);
        if (sd != "left" && sd != "right") { sendError(client, id, path, "Invalid side"); return; }
        if (v != "RED" && v != "YELLOW" && v != "GREEN") { sendError(client, id, path, "Invalid value"); return; }

        if (sd == "left") trafficLeft = v; else trafficRight = v;
        sendOk(client, id, path, [this](JsonObject p){ fillTrafficStatus(p); });

    } else {
        sendError(client, id, path, "Unknown SET path");
    }
}

void WebSocketServer::handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
    
    if (type == WS_EVT_CONNECT) {
        Serial.printf("Client %u connected\n", client->id());
        return;
    }
    if (type == WS_EVT_DISCONNECT) {
        Serial.printf("Client %u disconnected\n", client->id());
        return;
    }
    if (type != WS_EVT_DATA) return;

    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!(info->final && info->index == 0 && info->len == len)) {
        Serial.println("Fragmented frame ignored");
        return;
    }

    StaticJsonDocument<768> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        String id = doc["id"] | "";
        String path = doc["path"] | "";
        sendError(client, id, path.length() ? path : "/", "Invalid JSON");
        return;
    }

    int v = doc["v"] | 1;
    const char* typeStr = doc["type"] | "request";
    String id = doc["id"] | "";
    String method = doc["method"] | "";
    String path = doc["path"] | "";
    JsonVariant payload = doc["payload"];

    if (v != 1) { 
        sendError(client, id, path, "Unsupported protocol version"); 
        return; 
    }
    if (String(typeStr) != "request") { 
        return; 
    }

    if (method == "GET") {
        Serial.printf("[WS][RX] Client %u -> GET %s\n", client->id(), path.c_str());
        handleGet(client, id, path);
    }
    else if (method == "SET") {
        Serial.printf("[WS][RX] Client %u -> SET %s\n", client->id(), path.c_str());
        handleSet(client, id, path, payload);
    }
    else {
        sendError(client, id, path, "Unknown method");
    }
}

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