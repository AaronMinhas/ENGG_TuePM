#include "WebSocketServer.h"
#include "Logger.h"
#include "ConsoleCommands.h"

namespace {
  constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;
  constexpr unsigned long RETRY_DELAY_MS = 10000;
}

WebSocketServer::WebSocketServer(uint16_t port, StateWriter& stateWriter, CommandBus& commandBus, EventBus& eventBus) 
    : server(port), ws("/ws"), port(port), state_(stateWriter), commandBus_(commandBus), eventBus_(eventBus) {}

void WebSocketServer::attachConsole(ConsoleCommands* console) {
    console_ = console;
}

void WebSocketServer::configureWiFi(const char* ssid, const char* password) {
    ssid_ = ssid ? String(ssid) : String();
    password_ = password ? String(password) : String();
    wifiConfigured_ = ssid_.length() > 0;
    if (!wifiConfigured_) {
        LOG_INFO(Logger::TAG_WS, "WiFi configuration disabled (empty SSID)");
        return;
    }

    LOG_INFO(Logger::TAG_WS, "WiFi credentials set for network '%s'", ssid_.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    connectionInProgress_ = false;
    nextRetryMs_ = 0; // attempt immediately on next loop
}

void WebSocketServer::startServer() {
    if (serverStarted_) return;

    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        handleWsEvent(server, client, type, arg, data, len);
    });
    if (!handlersAttached_) {
        server.addHandler(&ws);

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "ESP32 WebSocket Server Running");
        });
        handlersAttached_ = true;
    }

    server.begin();
    serverStarted_ = true;

    LOG_INFO(Logger::TAG_WS, "WebSocket server started successfully!");
    LOG_INFO(Logger::TAG_WS, "Connect to: ws://%s/ws", WiFi.localIP().toString().c_str());

    // Subscribe to state-related events and broadcast snapshot on change
    if (!broadcastSubscribed_) {
        setupBroadcastSubscriptions();
        broadcastSubscribed_ = true;
    }
}

void WebSocketServer::networkLoop() {
    if (!wifiConfigured_) {
        return; // networking disabled
    }

    const unsigned long now = millis();
    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        if (!serverStarted_) {
            LOG_INFO(Logger::TAG_WS, "WiFi connected (IP: %s)", WiFi.localIP().toString().c_str());
            connectionInProgress_ = false;
            startServer();
        }
        return;
    }

    if (serverStarted_) {
        LOG_WARN(Logger::TAG_WS, "WiFi lost - closing WebSocket clients");
        ws.closeAll();
        server.end();
        serverStarted_ = false;
    }

    if (!connectionInProgress_) {
        if (now >= nextRetryMs_) {
            LOG_DEBUG(Logger::TAG_WS, "Attempting WiFi connection to %s", ssid_.c_str());
            WiFi.begin(ssid_.c_str(), password_.c_str());
            connectionInProgress_ = true;
            connectStartMs_ = now;
        }
        return;
    }

    // connection in progress
    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST) {
        LOG_WARN(Logger::TAG_WS, "WiFi connection failed - scheduling retry (status=%d)", static_cast<int>(status));
        WiFi.disconnect(true);
        connectionInProgress_ = false;
        nextRetryMs_ = now + RETRY_DELAY_MS;
        return;
    }

    if (now - connectStartMs_ > CONNECT_TIMEOUT_MS) {
        LOG_WARN(Logger::TAG_WS, "WiFi connection timeout - retrying");
        WiFi.disconnect(true);
        connectionInProgress_ = false;
        nextRetryMs_ = now + RETRY_DELAY_MS;
    }
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

void WebSocketServer::fillVehicleTrafficStatus(JsonObject obj) {
    state_.fillVehicleTrafficStatus(obj);
}

void WebSocketServer::fillSystemStatus(JsonObject obj) {
    state_.fillSystemStatus(obj);
}

void WebSocketServer::broadcastSnapshot() {
    DynamicJsonDocument doc(1024);
    state_.buildSnapshot(doc);
    String out; serializeJson(doc, out);
    ws.textAll(out);
}

void WebSocketServer::setupBroadcastSubscriptions() {
    using E = BridgeEvent;
    auto sub = [this](EventData*){
        broadcastSnapshot();
    };

    // Subscribe to the same set StateWriter uses
    eventBus_.subscribe(E::BOAT_DETECTED, sub);
    eventBus_.subscribe(E::BOAT_DETECTED_LEFT, sub);
    eventBus_.subscribe(E::BOAT_DETECTED_RIGHT, sub);
    eventBus_.subscribe(E::BOAT_PASSED, sub);
    eventBus_.subscribe(E::BOAT_PASSED_LEFT, sub);
    eventBus_.subscribe(E::BOAT_PASSED_RIGHT, sub);
    eventBus_.subscribe(E::FAULT_DETECTED, sub);
    eventBus_.subscribe(E::FAULT_CLEARED, sub);
    eventBus_.subscribe(E::MANUAL_OVERRIDE_ACTIVATED, sub);
    eventBus_.subscribe(E::MANUAL_OVERRIDE_DEACTIVATED, sub);

    eventBus_.subscribe(E::TRAFFIC_STOPPED_SUCCESS, sub);
    eventBus_.subscribe(E::BRIDGE_OPENED_SUCCESS, sub);
    eventBus_.subscribe(E::BRIDGE_CLOSED_SUCCESS, sub);
    eventBus_.subscribe(E::TRAFFIC_RESUMED_SUCCESS, sub);
    eventBus_.subscribe(E::INDICATOR_UPDATE_SUCCESS, sub);
    eventBus_.subscribe(E::SYSTEM_SAFE_SUCCESS, sub);
    eventBus_.subscribe(E::CAR_LIGHT_CHANGED_SUCCESS, sub);
    eventBus_.subscribe(E::BOAT_LIGHT_CHANGED_SUCCESS, sub);
    eventBus_.subscribe(E::TRAFFIC_COUNT_CHANGED, sub);

    // Also include manual requests so frontend reflects transitional states immediately
    eventBus_.subscribe(E::MANUAL_BRIDGE_OPEN_REQUESTED, sub);
    eventBus_.subscribe(E::MANUAL_BRIDGE_CLOSE_REQUESTED, sub);
    eventBus_.subscribe(E::MANUAL_TRAFFIC_STOP_REQUESTED, sub);
    eventBus_.subscribe(E::MANUAL_TRAFFIC_RESUME_REQUESTED, sub);
    
    // Subscribe to state changes for immediate updates
    eventBus_.subscribe(E::STATE_CHANGED, sub);
}

/**
 * This function is used to build and send a success JSON message back to the Frontend if the command was
 * successful. To indicate that it was successful, we use "ok" = true, and include relevant fillPayload
 * data and this function will only be called if the operation was successful.
 */
void WebSocketServer::sendOk(AsyncWebSocketClient* client, const String& id, const String& path,
                             std::function<void(JsonObject)> fillPayload) {
    DynamicJsonDocument doc(512);
    doc["v"] = 1;
    doc["id"] = id;
    doc["type"] = "response";
    doc["ok"] = true;
    doc["path"] = path;
    if (fillPayload) {
        JsonObject payload = doc["payload"].to<JsonObject>();
        fillPayload(payload);
    }
    String out; serializeJson(doc, out);
    client->text(out);

    // Only log important SET commands, not routine status requests
    if (path.startsWith("/bridge/state")) {
        LOG_DEBUG(Logger::TAG_WS, "[TX][OK] Client %u <- %s", client->id(), path.c_str());
    }
}

void WebSocketServer::sendError(AsyncWebSocketClient* client, const String& id, const String& path, const String& msg) {
    DynamicJsonDocument doc(384);
    doc["v"] = 1;
    doc["id"] = id;
    doc["type"] = "response";
    doc["ok"] = false;
    doc["path"] = path;
    doc["error"] = msg;
    String out; serializeJson(doc, out);
    client->text(out);

    LOG_WARN(Logger::TAG_WS, "[TX][ERR] Client %u <- %s error=%s", client->id(), path.c_str(), msg.c_str());
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
    } else if (path == "/traffic/vehicles/status") {
        sendOk(client, id, path, [this](JsonObject p){ fillVehicleTrafficStatus(p); });
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
 *      /traffic/boat/light
 *      /system/reset
 *
 * The information received by these endpoints will then attempt to apply this to the BridgeStateMachine.
 */
void WebSocketServer::handleSet(AsyncWebSocketClient* client, const String& id, const String& path, JsonVariant payload) {
    if (path == "/bridge/state") {
        auto payloadObj = payload.as<JsonObject>();
        const char* state = payloadObj["state"];
        if (!state) { sendError(client, id, path, "Missing 'state'"); return; }
        
        String s = String(state);
        if (s != "Open" && s != "Closed") { sendError(client, id, path, "Invalid state"); return; }

        // Send manual control event via EventBus to StateMachine (Command Mode)
        if (s == "Open") {
            LOG_INFO(Logger::TAG_WS, "Bridge open requested via WebSocket");
            // Allocate on heap as EventBus processes asynchronously
            auto* eventData = new SimpleEventData(BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED);
            eventBus_.publish(BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED, eventData);
        } else {
            LOG_INFO(Logger::TAG_WS, "Bridge close requested via WebSocket");
            auto* eventData = new SimpleEventData(BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED);
            eventBus_.publish(BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED, eventData);
        }

        // Acknowledge request and provide current vs requested state distinctly
        sendOk(client, id, path, [this, s](JsonObject p){
            p["requestedState"] = s;
            JsonObject current = p["current"].to<JsonObject>();
            fillBridgeStatus(current);
        });

    } else if (path == "/traffic/car") {
        auto payloadObj = payload.as<JsonObject>();
        const char* value = payloadObj["value"];
        if (!value) { sendError(client, id, path, "Missing 'value'"); return; }

        String v = String(value);
        if (v != "Red" && v != "Yellow" && v != "Green") { sendError(client, id, path, "Invalid value"); return; }

        // Send car traffic command via CommandBus
        Command cmd;
        cmd.target = CommandTarget::SIGNAL_CONTROL;
        cmd.action = CommandAction::SET_CAR_TRAFFIC;
        cmd.data = v;  // Pass the colour (Red/Yellow/Green)
        
        LOG_INFO(Logger::TAG_WS, "Publishing car traffic command - Value: %s", v.c_str());
        
        commandBus_.publish(cmd);

        // Acknowledge request and include current snapshot
        sendOk(client, id, path, [this, v](JsonObject p){
            p["requestedValue"] = v;
            JsonObject current = p["current"].to<JsonObject>();
            fillCarTrafficStatus(current);
        });

    } else if (path == "/traffic/boat/light") { 
        auto payloadObj = payload.as<JsonObject>();
        const char* side = payloadObj["side"];
        const char* value = payloadObj["value"];
        if (!side || !value) { sendError(client, id, path, "Missing 'side' or 'value'"); return; }

        String sd = String(side), v = String(value);
        if (sd != "left" && sd != "right") { sendError(client, id, path, "Invalid side"); return; }
        if (v != "Red" && v != "Green") { sendError(client, id, path, "Invalid value"); return; }

        // Send specific boat light command via CommandBus
        CommandAction action;
        if (sd == "left") {
            action = CommandAction::SET_BOAT_LIGHT_LEFT;
        } else {
            action = CommandAction::SET_BOAT_LIGHT_RIGHT;
        }
        
        Command cmd;
        cmd.target = CommandTarget::SIGNAL_CONTROL;
        cmd.action = action;
        cmd.data = v;  // Pass the colour (Red/Green)
        
        LOG_INFO(Logger::TAG_WS, "Publishing boat light command - Side: %s, Value: %s",
                 sd.c_str(), v.c_str());
        
        commandBus_.publish(cmd);

        // Acknowledge request and include current snapshot
        sendOk(client, id, path, [this, sd, v](JsonObject p){
            p["requestedSide"] = sd;
            p["requestedValue"] = v;
            JsonObject current = p["current"].to<JsonObject>();
            fillBoatTrafficStatus(current);
        });
    } else if (path == "/system/reset") {
        LOG_WARN(Logger::TAG_WS, "System reset requested via WebSocket client %u", client ? client->id() : 0);

        auto* resetData = new SimpleEventData(BridgeEvent::SYSTEM_RESET_REQUESTED);
        eventBus_.publish(BridgeEvent::SYSTEM_RESET_REQUESTED, resetData, EventPriority::EMERGENCY);

        sendOk(client, id, path, [this](JsonObject p){
            JsonObject bridge = p["bridge"].to<JsonObject>();
            fillBridgeStatus(bridge);
            JsonObject car = p["carTraffic"].to<JsonObject>();
            fillCarTrafficStatus(car);
            JsonObject boat = p["boatTraffic"].to<JsonObject>();
            fillBoatTrafficStatus(boat);
            JsonObject vehicles = p["trafficCounts"].to<JsonObject>();
            fillVehicleTrafficStatus(vehicles);
        });
    } else if (path == "/console/command") {
        if (!console_) { sendError(client, id, path, "Console unavailable"); return; }
        if (!payload.is<JsonObject>()) { sendError(client, id, path, "Invalid payload"); return; }
        auto payloadObj = payload.as<JsonObject>();
        const char* cmd = payloadObj["command"];
        if (!cmd) { sendError(client, id, path, "Missing 'command'"); return; }

        String raw = String(cmd);
        if (raw.length() == 0) {
            sendError(client, id, path, "Command cannot be empty");
            return;
        }

        LOG_INFO(Logger::TAG_WS, "Console command requested via WebSocket: %s", raw.c_str());
        const bool handled = console_->executeCommand(raw);
        sendOk(client, id, path, [raw, handled](JsonObject p){
            p["command"] = raw;
            p["handled"] = handled;
        });
    } else {
        sendError(client, id, path, "Unknown SET path");
    }
}

void WebSocketServer::handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
    
    if (type == WS_EVT_CONNECT) {
        LOG_INFO(Logger::TAG_WS, "Client %u connected", client->id());
        return;
    }
    if (type == WS_EVT_DISCONNECT) {
        LOG_INFO(Logger::TAG_WS, "Client %u disconnected", client->id());
        return;
    }
    if (type != WS_EVT_DATA) return;

    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!(info->final && info->index == 0 && info->len == len)) {
        LOG_DEBUG(Logger::TAG_WS, "Fragmented frame ignored");
        return;
    }

    DynamicJsonDocument doc(768);
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        LOG_WARN(Logger::TAG_WS, "JSON parse error: %s", err.c_str());
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
        // Only log non routine GET requests
        if (path != "/bridge/status" && path != "/traffic/car/status" && 
            path != "/traffic/boat/status" && path != "/system/status" && path != "/system/ping") {
            LOG_DEBUG(Logger::TAG_WS, "[RX] Client %u -> GET %s", client->id(), path.c_str());
        }
        handleGet(client, id, path);
    }
    else if (method == "SET") {
        LOG_DEBUG(Logger::TAG_WS, "[RX] Client %u -> SET %s", client->id(), path.c_str());
        handleSet(client, id, path, payload);
    }
    else {
        sendError(client, id, path, "Unknown method");
    }
}
