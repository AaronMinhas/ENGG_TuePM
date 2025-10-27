#include "Logger.h"

#include <cstdarg>

namespace Logger {

namespace {
  Level currentLevel = Level::INFO;
  constexpr size_t LOG_BUFFER_SIZE = 256;
}

const char* const TAG_SYS = "SYS";
const char* const TAG_MC  = "MC";
const char* const TAG_SC  = "SC";
const char* const TAG_DS  = "DS";
const char* const TAG_FSM = "FSM";
const char* const TAG_CMD = "CMD";
const char* const TAG_WS  = "WS";
const char* const TAG_LOC = "LOC";
const char* const TAG_CON = "CON";
const char* const TAG_EVT = "EVT";
const char* const TAG_SAFE = "SAFE";
const char* const TAG_TRF = "TRF";

void begin(Level defaultLevel) {
  currentLevel = defaultLevel;
}

void setLevel(Level level) {
  currentLevel = level;
}

Level getLevel() {
  return currentLevel;
}

const char* levelToString(Level level) {
  switch (level) {
    case Level::DEBUG: return "DEBUG";
    case Level::INFO:  return "INFO";
    case Level::WARN:  return "WARN";
    case Level::ERROR: return "ERROR";
    default:           return "NONE";
  }
}

static void writeLine(Level level, const char* tag, const char* message) {
  Serial.printf("[%s][%s] %s\n", levelToString(level), tag ? tag : "GEN", message);
}

void logf(Level level, const char* tag, const char* fmt, ...) {
  if (level < currentLevel) return;
  char buffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, LOG_BUFFER_SIZE, fmt, args);
  va_end(args);
  writeLine(level, tag, buffer);
}

void log(Level level, const char* tag, const String& message) {
  if (level < currentLevel) return;
  writeLine(level, tag, message.c_str());
}

} // namespace Logger
