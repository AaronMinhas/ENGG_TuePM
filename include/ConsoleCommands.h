#pragma once

#include <Arduino.h>

class MotorControl;
class DetectionSystem;

// Centralised CLI command router
class ConsoleCommands {
public:
  ConsoleCommands(MotorControl& motor, DetectionSystem& detect);

  void begin();
  void poll();   // non-blocking; call frequently

private:
  MotorControl& motor_;
  DetectionSystem& detect_;

  void handleCommand(const String& cmd);
  void printHelp();
  void printStatus();

  // Streaming of ultrasonic readings (no timestamps)
  static constexpr uint8_t STREAM_LEFT  = 0x01;
  static constexpr uint8_t STREAM_RIGHT = 0x02;
  uint8_t streamMask_ = 0; // bitmask tracking which ultrasonic streams are active
  unsigned long streamIntervalMs_ = 100; // default 10 Hz
  unsigned long lastStreamMs_ = 0;
  void handleStreaming();
};
