#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>
#include <vector>
#include "EventBus.h"
#include "BridgeSystemDefs.h"

class ConsoleCommands;

class StateWriter {
public:
  explicit StateWriter(EventBus& bus);

  void beginSubscriptions();
  void attachConsole(ConsoleCommands* console);

  void fillBridgeStatus(JsonObject obj) const;
  void fillCarTrafficStatus(JsonObject obj) const;
  void fillBoatTrafficStatus(JsonObject obj) const;
  void fillSystemStatus(JsonObject obj) const;

  void buildSnapshot(JsonDocument& out) const;

  std::vector<String> getActivityLog() const;

private:
  EventBus& bus_;
  ConsoleCommands* console_ = nullptr;
  mutable std::mutex mu_;

  String bridgeState_ = "IDLE";
  bool bridgeLockEngaged_ = true;
  uint32_t bridgeLastChangeMs_ = 0;

  String carLeft_ = "Green";
  String carRight_ = "Green";
  
  String boatLeft_ = "Red";
  String boatRight_ = "Red";
  
  // Boat green period timer tracking
  uint32_t boatTimerStartMs_ = 0;  // When boat green period started (0 = inactive)
  String boatTimerSide_ = "";       // Which side has green ("left" or "right", empty if inactive)

  bool inFault_ = false;
  bool manualMode_ = false;
  bool simulationMode_ = false;
  bool simUltrasonicLeftEnabled_ = false;
  bool simUltrasonicRightEnabled_ = false;
  bool simBeamBreakEnabled_ = false;

  static constexpr size_t LOG_CAP_ = 64;
  std::vector<String> log_;
  uint32_t logSeq_ = 0;

  void onEvent(EventData* data);
  void applyEvent(BridgeEvent ev, EventData* data);
  void pushLog(const String& line);

  static const char* eventName(BridgeEvent ev);
  static const char* stateToString(BridgeState state);
};
