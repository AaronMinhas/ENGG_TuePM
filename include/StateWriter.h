#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>
#include <vector>
#include "EventBus.h"
#include "BridgeSystemDefs.h"

class StateWriter {
public:
  explicit StateWriter(EventBus& bus);

  void beginSubscriptions();

  void fillBridgeStatus(JsonObject obj) const;
  void fillCarTrafficStatus(JsonObject obj) const;
  void fillBoatTrafficStatus(JsonObject obj) const;
  void fillSystemStatus(JsonObject obj) const;

  void buildSnapshot(JsonDocument& out) const;

  std::vector<String> getActivityLog() const;

private:
  EventBus& bus_;
  mutable std::mutex mu_;

  String bridgeState_ = "IDLE";
  bool bridgeLockEngaged_ = true;
  uint32_t bridgeLastChangeMs_ = 0;

  String carLeft_ = "Green";
  String carRight_ = "Green";
  
  String boatLeft_ = "Red";
  String boatRight_ = "Red";

  bool inFault_ = false;
  bool manualMode_ = false;

  static constexpr size_t LOG_CAP_ = 64;
  std::vector<String> log_;

  void onEvent(EventData* data);
  void applyEvent(BridgeEvent ev, EventData* data);
  void pushLog(const String& line);

  static const char* eventName(BridgeEvent ev);
  static const char* stateToString(BridgeState state);
};