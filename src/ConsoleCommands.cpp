#include "ConsoleCommands.h"
#include "MotorControl.h"
#include "DetectionSystem.h"
#include "EventBus.h"
#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include "Logger.h"

ConsoleCommands::ConsoleCommands(MotorControl &motor, DetectionSystem &detect, EventBus& eventBus, SignalControl& signalControl)
    : motor_(motor), detect_(detect), eventBus_(eventBus), signalControl_(signalControl) {}

void ConsoleCommands::begin()
{
  LOG_INFO(Logger::TAG_CON, "Commands ready. Type 'help' for options.");
}

void ConsoleCommands::poll()
{
  handleStreaming();

  if (!Serial.available())
      return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0)
      return;

  String lower = cmd;
  lower.toLowerCase();
  handleCommand(lower);
  // After handling any command, also process streaming if enabled
  handleStreaming();
}

bool ConsoleCommands::executeCommand(const String& raw)
{
  String cmd = raw;
  cmd.trim();
  if (cmd.length() == 0)
    return false;
  
  String lower = cmd;
  lower.toLowerCase();
  return handleCommand(lower);
}

bool ConsoleCommands::handleCommand(const String& cmd) {
  // Global simulation toggles
  if (cmd == "sim on" || cmd == "simulation on")
  {
    // Enable simulation mode for both sensors and motor control
    detect_.setSimulationMode(true);
    motor_.setSimulationMode(true);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE ENABLED (sensors + motor control)");
    auto* simOn = new SimpleEventData(BridgeEvent::SIMULATION_ENABLED);
    eventBus_.publish(BridgeEvent::SIMULATION_ENABLED, simOn, EventPriority::NORMAL);
    return true;
  }
  if (cmd == "sim off" || cmd == "simulation off")
  {
    detect_.setSimulationMode(false);
    motor_.setSimulationMode(false);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE DISABLED (sensors + motor control)");
    auto* simOff = new SimpleEventData(BridgeEvent::SIMULATION_DISABLED);
    eventBus_.publish(BridgeEvent::SIMULATION_DISABLED, simOff, EventPriority::NORMAL);
    auto* resetData = new SimpleEventData(BridgeEvent::SYSTEM_RESET_REQUESTED);
    eventBus_.publish(BridgeEvent::SYSTEM_RESET_REQUESTED, resetData, EventPriority::EMERGENCY);
    LOG_INFO(Logger::TAG_CON, "System reset requested after exiting simulation mode");
    return true;
  }
  
  // Motor commands (mirroring existing strings)
  // These commands trigger manual mode via the state machine
  if (cmd == "raise" || cmd == "r") {
    auto* eventData = new SimpleEventData(BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED);
    eventBus_.publish(BridgeEvent::MANUAL_BRIDGE_OPEN_REQUESTED, eventData);
    LOG_INFO(Logger::TAG_CON, "Console: Manual bridge open requested");
    return true;
  }
  if (cmd == "lower" || cmd == "l") {
    auto* eventData = new SimpleEventData(BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED);
    eventBus_.publish(BridgeEvent::MANUAL_BRIDGE_CLOSE_REQUESTED, eventData);
    LOG_INFO(Logger::TAG_CON, "Console: Manual bridge close requested");
    return true;
  }
  if (cmd == "halt"  || cmd == "h" || cmd == "stop") { motor_.halt(); return true; }


  // Test boat event commands
  if (cmd == "test boat left" || cmd == "tbl") {
    auto* eventData = new BoatEventData(BridgeEvent::BOAT_DETECTED_LEFT, BoatEventSide::LEFT);
    eventBus_.publish(BridgeEvent::BOAT_DETECTED_LEFT, eventData);
    LOG_INFO(Logger::TAG_CON, "TEST: Simulated boat detected from LEFT side");
    return true;
  }
  if (cmd == "test boat right" || cmd == "tbr") {
    auto* eventData = new BoatEventData(BridgeEvent::BOAT_DETECTED_RIGHT, BoatEventSide::RIGHT);
    eventBus_.publish(BridgeEvent::BOAT_DETECTED_RIGHT, eventData);
    LOG_INFO(Logger::TAG_CON, "TEST: Simulated boat detected from RIGHT side");
    return true;
  }
  if (cmd == "test boat pass" || cmd == "tbp") {
    // Trigger generic boat passed event (direction-agnostic)
    // In real operation, beam break sensor determines direction automatically
    auto* eventData = new BoatEventData(BridgeEvent::BOAT_PASSED, BoatEventSide::LEFT);
    eventBus_.publish(BridgeEvent::BOAT_PASSED, eventData);
    LOG_INFO(Logger::TAG_CON, "TEST: Simulated boat cleared channel (beam break)");
    return true;
  }

  // Light control commands
  if (cmd.startsWith("car light ") || cmd.startsWith("cl ")) {
    String color = cmd.substring(cmd.indexOf(' ') + 1);
    color.trim();
    color.toLowerCase();
    if (color == "red" || color == "yellow" || color == "green") {
      signalControl_.setCarTraffic(color);
      LOG_INFO(Logger::TAG_CON, "LIGHT CONTROL: Car lights set to %s", color.c_str());
    } else {
      LOG_WARN(Logger::TAG_CON, "Invalid car light color. Use: red, yellow, green");
    }
    return true;
  }
  
  if (cmd.startsWith("boat light ") || cmd.startsWith("bl ")) {
    String rest = cmd.substring(cmd.indexOf(' ') + 1);
    rest.trim();
    int spacePos = rest.indexOf(' ');
    if (spacePos > 0) {
      String side = rest.substring(0, spacePos);
      String color = rest.substring(spacePos + 1);
      side.trim();
      color.trim();
      side.toLowerCase();
      color.toLowerCase();
      
      if ((side == "left" || side == "right") && (color == "red" || color == "green")) {
        signalControl_.setBoatLight(side, color);
        LOG_INFO(Logger::TAG_CON, "LIGHT CONTROL: Boat light %s set to %s", side.c_str(), color.c_str());
      } else {
        LOG_WARN(Logger::TAG_CON, "Invalid boat light command. Use: 'boat light <left|right> <red|green>'");
      }
    } else {
      LOG_WARN(Logger::TAG_CON, "Invalid boat light command. Use: 'boat light <left|right> <red|green>'");
    }
    return true;
  }
  
  if (cmd == "lights status" || cmd == "ls") {
    LOG_INFO(Logger::TAG_CON, "LIGHT CONTROL: Use 'status' command to see current system state");
    return true;
  }

  // Limit switch diagnostics
  if (cmd == "limit") {
    const int raw = motor_.getLimitSwitchRaw();
    const bool active = motor_.isLimitSwitchActive();
    LOG_INFO(Logger::TAG_MC, "LIMIT SWITCH (shared): raw=%d, active=%s", raw, active ? "YES" : "NO");
    return true;
  }
  if (cmd == "lsw") {
    const bool enable = (limitStreamEnabled_ == false);
    limitStreamEnabled_ = enable;
    LOG_INFO(Logger::TAG_MC, "LIMIT SWITCH STREAM: %s", enable ? "enabled" : "disabled");
    return true;
  }
  
  // Test limit switch press (simulates motor reaching limit)
  if (cmd == "test limit" || cmd == "tl") {
    LOG_INFO(Logger::TAG_CON, "TEST: Simulating limit switch press");
    motor_.simulateLimitSwitchPress();
    return true;
  }

  // Ultrasonic/detection commands
  // Short toggle: 'us' toggles streaming (both sensors); 'us state' prints status
  if (cmd == "us")
  {
    const bool enable = streamMask_ != (STREAM_LEFT | STREAM_RIGHT);
    streamMask_ = enable ? (STREAM_LEFT | STREAM_RIGHT) : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (both sensors)", enable ? "enabled" : "disabled");
    return true;
  }
  if (cmd == "usl")
  {
    const bool enable = streamMask_ != STREAM_LEFT;
    streamMask_ = enable ? STREAM_LEFT : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (left sensor)", enable ? "enabled" : "disabled");
    return true;
  }
  if (cmd == "usr")
  {
    const bool enable = streamMask_ != STREAM_RIGHT;
    streamMask_ = enable ? STREAM_RIGHT : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (right sensor)", enable ? "enabled" : "disabled");
    return true;
  }

  if (cmd.startsWith("log level "))
  {
    String levelStr = cmd.substring(String("log level ").length());
    levelStr.trim();
    levelStr.toLowerCase();
    Logger::Level newLevel = Logger::Level::INFO;
    bool matched = true;
    if (levelStr == "debug")
      newLevel = Logger::Level::DEBUG;
    else if (levelStr == "info")
      newLevel = Logger::Level::INFO;
    else if (levelStr == "warn" || levelStr == "warning")
      newLevel = Logger::Level::WARN;
    else if (levelStr == "error")
      newLevel = Logger::Level::ERROR;
    else if (levelStr == "none")
      newLevel = Logger::Level::NONE;
    else
      matched = false;

    if (matched)
    {
      Logger::setLevel(newLevel);
      LOG_INFO(Logger::TAG_CON, "Log level set to %s", Logger::levelToString(newLevel));
    }
    else
    {
      LOG_WARN(Logger::TAG_CON, "Unknown log level '%s'", levelStr.c_str());
    }
    return matched;
  }

  if (cmd == "status" || cmd == "mode") { printStatus(); return true; }
  if (cmd == "help" || cmd == "?") { printHelp(); return true; }

  LOG_WARN(Logger::TAG_CON, "Unknown command. Type 'help' for available commands.");
  return false;
}

void ConsoleCommands::printHelp()
{
  Serial.println("Available commands:");
  Serial.println("  sim on / simulation on    - Enable simulation (motor + ultrasonic)");
  Serial.println("  sim off / simulation off  - Disable simulation (motor + ultrasonic)");
  Serial.println("  raise|r / lower|l / halt|h / stop");
  Serial.println("  us                        - Toggle ultrasonic streaming for both sensors");
  Serial.println("  usl                       - Toggle ultrasonic streaming for left sensor only");
  Serial.println("  usr                       - Toggle ultrasonic streaming for right sensor only");
  Serial.println("  === Test/Simulation Commands ===");
  Serial.println("  test boat left|tbl       - Simulate boat detected from LEFT");
  Serial.println("  test boat right|tbr      - Simulate boat detected from RIGHT");
  Serial.println("  test boat pass|tbp       - Simulate boat cleared channel (beam break)");
  Serial.println("  test limit|tl            - Simulate limit switch press (triggers normal stop)");
  Serial.println("  car light <colour>|cl <colour>  - Set car lights (red/yellow/green)");
  Serial.println("  boat light <side> <colour>|bl <side> <colour>  - Set boat lights (left/right, red/green)");
  Serial.println("  lights status|ls          - Show light control status");
  Serial.println("  log level <lvl>           - Set log level (debug/info/warn/error/none)");
  Serial.println("  status|mode               - Show combined status");
  Serial.println("  help|?                    - Show this help");
}

void ConsoleCommands::printStatus()
{
  LOG_INFO(Logger::TAG_MC, "MOTOR CONTROL: Mode: %s", motor_.isSimulationMode() ? "SIMULATION" : "REAL");

  // Print both left and right sensor status
  const float leftDist = detect_.getLeftFilteredDistanceCm();
  const float rightDist = detect_.getRightFilteredDistanceCm();

  LOG_INFO(Logger::TAG_DS, "ULTRASONIC_LEFT: Mode: %s, distance: %s, zone: %s",
           detect_.isSimulationMode() ? "SIM" : "REAL",
           (leftDist > 0 ? String(leftDist, 1).c_str() : "unknown"),
           detect_.getLeftZoneName());

  LOG_INFO(Logger::TAG_DS, "ULTRASONIC_RIGHT: Mode: %s, distance: %s, zone: %s",
           detect_.isSimulationMode() ? "SIM" : "REAL",
           (rightDist > 0 ? String(rightDist, 1).c_str() : "unknown"),
           detect_.getRightZoneName());

  if (detect_.isSimulationMode())
  {
    auto simConfig = detect_.getSimulationSensorConfig();
    LOG_INFO(Logger::TAG_DS, "SIM SENSORS: ultrasonicLeft=%s, ultrasonicRight=%s, beamBreak=%s",
             simConfig.ultrasonicLeftEnabled ? "ENABLED" : "DISABLED",
             simConfig.ultrasonicRightEnabled ? "ENABLED" : "DISABLED",
             simConfig.beamBreakEnabled ? "ENABLED" : "DISABLED");
  }
}

void ConsoleCommands::handleStreaming()
{
  const unsigned long now = millis();
  bool any = (streamMask_ != 0) || limitStreamEnabled_;
  if (!any) return;
  if (now - lastStreamMs_ < streamIntervalMs_) return;
  lastStreamMs_ = now;

  // Ensure sensor gets a chance to read
  detect_.update();

  if (streamMask_ & STREAM_LEFT)
  {
    const float leftDist = detect_.getLeftFilteredDistanceCm();
    LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_LEFT: dist=%s cm, zone=%s, mode=%s",
              (leftDist > 0 ? String(leftDist, 1).c_str() : "unknown"),
              detect_.getLeftZoneName(),
              detect_.isSimulationMode() ? "SIM" : "REAL");
  }

  if (streamMask_ & STREAM_RIGHT)
  {
    const float rightDist = detect_.getRightFilteredDistanceCm();
    LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_RIGHT: dist=%s cm, zone=%s, mode=%s",
              (rightDist > 0 ? String(rightDist, 1).c_str() : "unknown"),
              detect_.getRightZoneName(),
              detect_.isSimulationMode() ? "SIM" : "REAL");
  }

  if (limitStreamEnabled_)
  {
    const int raw = motor_.getLimitSwitchRaw();
    const bool active = motor_.isLimitSwitchActive();
    LOG_DEBUG(Logger::TAG_MC, "LIMIT SWITCH: raw=%d active=%s", raw, active ? "YES" : "NO");
  }
}
