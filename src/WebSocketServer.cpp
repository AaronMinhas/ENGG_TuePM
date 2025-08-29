#include "WebSocketServer.h"

WebSocketServer::WebSocketServer(uint16_t port, StateWriter& stateWriter) 
    : server(port), ws("/ws"), port(port), lastStatusCheck(0), state_(stateWriter) {}

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
    state_.fillBridgeStatus(obj);
}

void WebSocketServer::fillCarTrafficStatus(JsonObject obj) {
    state_.fillCarTrafficStatus(obj);
}

void WebSocketServer::fillBoatTrafficStatus(JsonObject obj) {
    state_.fillBoatTrafficStatus(obj);
}

void WebSocketServer::fillSystemStatus(JsonObject obj) {
    state_.fillSystemStatus(obj);
}

/**
 * This function is used to build and send a success JSON message back to the Frontend if the command was
 * successful. To indicate that it was successful, we use "ok" = true, and include relevant fillPayload
 * data and this function will only be called if the operation was successful.
 */
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

/**
 * This function handles GET paths. These endpoints defined below handle requests from the frontend. Each endpoint
 * is linked to a specific component that has their state saved in the StateWriter. Each endpoint must collect the
 * current state from the StateWriter and then send it back to the frontend that called the GET request.
 * 
 * Currently we have these endpoints:
 * 
 *      /bridge/status
 *      /traffic/car/status
 *      /traffic/boat/status
 *      /system/status
 *      /system/ping
 *
 * The information received by these endpoints will then attempt to apply this to the BridgeStateMachine.
 */
void WebSocketServer::handleGet(AsyncWebSocketClient* client, const String& id, const String& path) {
    if (path == "/bridge/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillBridgeStatus(p); });
    } else if (path == "/traffic/car/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillCarTrafficStatus(p); });
    } else if (path == "/traffic/boat/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillBoatTrafficStatus(p); });
    } else if (path == "/system/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillSystemStatus(p); });
    } else if (path == "/system/ping") {
        sendOk(client, id, path, [](JsonObject p){ p["nowMs"] = millis(); });
    } else {
        sendError(client, id, path, "Unknown GET path");
    }
}

/**
 * This function handles SET paths. These endpoints defined below determine what components are controlled
 * by the frontend. Currently, we have these endpoints:
 * 
 *      /bridge/state
 *      /traffic/car/light
 *      /traffic/boat/light
 *
 * The information received by these endpoints will then attempt to apply this to the BridgeStateMachine.
 */
void WebSocketServer::handleSet(AsyncWebSocketClient* client, const String& id, const String& path, JsonVariant payload) {
    if (path == "/bridge/state") {
        // auto payloadObj = payload.as<JsonObject>();
        // const char* state = payloadObj["state"];
        // if (!state) { sendError(client, id, path, "Missing 'state'"); return; }
        // String s = String(state);
        // if (s != "Open" && s != "Closed") { sendError(client, id, path, "Invalid state"); return; }

        // bridgeState = s;
        // lockEngaged = (s == "Closed");
        // bridgeLastChangeMs = millis();

        sendOk(client, id, path, [this](JsonObject p){ fillBridgeStatus(p); });

    } else if (path == "/traffic/car/light") {
        // auto payloadObj = payload.as<JsonObject>();
        // const char* side = payloadObj["side"];
        // const char* value = payloadObj["value"];
        // if (!side || !value) { sendError(client, id, path, "Missing 'side' or 'value'"); return; }

        // String sd = String(side), v = String(value);
        // if (sd != "left" && sd != "right") { sendError(client, id, path, "Invalid side"); return; }
        // if (v != "Red" && v != "Yellow" && v != "Green") { sendError(client, id, path, "Invalid value"); return; }

        // if (sd == "left") carTrafficLeft = v; else carTrafficRight = v;

        sendOk(client, id, path, [this](JsonObject p){ fillCarTrafficStatus(p); });

    } else if (path == "/traffic/boat/light") { 
        // auto payloadObj = payload.as<JsonObject>();
        // const char* side = payloadObj["side"];
        // const char* value = payloadObj["value"];
        // if (!side || !value) { sendError(client, id, path, "Missing 'side' or 'value'"); return; }

        // String sd = String(side), v = String(value);
        // if (sd != "left" && sd != "right") { sendError(client, id, path, "Invalid side"); return; }
        // if (v != "Red" && v != "Green") { sendError(client, id, path, "Invalid value"); return; }

        // if (sd == "left") boatTrafficLeft = v; else boatTrafficRight = v;
        sendOk(client, id, path, [this](JsonObject p){ fillBoatTrafficStatus(p); });
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
            Serial.print(WiFi.localIP());
        }
    }
}