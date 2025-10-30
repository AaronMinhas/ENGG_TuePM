#pragma once

#include <Arduino.h>

class MotorControl;
class DetectionSystem;
class EventBus;
class SignalControl;

// Centralised CLI command router
class ConsoleCommands {
public:
  ConsoleCommands(MotorControl& motor, DetectionSystem& detect, EventBus& eventBus, SignalControl& signalControl);

  void begin();
  void poll();   // non-blocking; call frequently
  bool executeCommand(const String& raw);  // exposed for remote invocation

private:
  MotorControl& motor_;
  DetectionSystem& detect_;
  EventBus& eventBus_;
  SignalControl& signalControl_;

  bool handleCommand(const String& cmd);
  void printHelp();
  void printStatus();

  // Streaming of ultrasonic readings (no timestamps)
  static constexpr uint8_t STREAM_LEFT  = 0x01;
  static constexpr uint8_t STREAM_RIGHT = 0x02;
  uint8_t streamMask_ = 0; // bitmask tracking which ultrasonic streams are active
  unsigned long streamIntervalMs_ = 100; // default 10 Hz
  unsigned long lastStreamMs_ = 0;
  bool limitStreamEnabled_ = false;
  void handleStreaming();
};
