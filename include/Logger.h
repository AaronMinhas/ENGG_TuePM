#pragma once

#include <Arduino.h>

namespace Logger {

enum class Level : uint8_t {
  DEBUG = 0,
  INFO,
  WARN,
  ERROR,
  NONE
};

// Common subsystem tags
extern const char* const TAG_SYS;  // System / setup
extern const char* const TAG_MC;   // Motor control
extern const char* const TAG_SC;   // Signal control
extern const char* const TAG_DS;   // Detection system
extern const char* const TAG_FSM;  // State machine
extern const char* const TAG_CMD;  // Controller / command bus
extern const char* const TAG_WS;   // WiFi / WebSocket
extern const char* const TAG_LOC;  // Local state indicator
extern const char* const TAG_CON;  // Console interface
extern const char* const TAG_EVT;  // Event bus
extern const char* const TAG_SAFE; 
extern const char* const TAG_TRF;  // Traffic counter / sensors

void begin(Level defaultLevel = Level::INFO);
void setLevel(Level level);
Level getLevel();
const char* levelToString(Level level);

void logf(Level level, const char* tag, const char* fmt, ...);
void log(Level level, const char* tag, const String& message);

} // namespace Logger

#define LOG_DEBUG(tag, fmt, ...) Logger::logf(Logger::Level::DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)  Logger::logf(Logger::Level::INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  Logger::logf(Logger::Level::WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) Logger::logf(Logger::Level::ERROR, tag, fmt, ##__VA_ARGS__)
