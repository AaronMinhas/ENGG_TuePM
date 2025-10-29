#include "ConsoleCommands.h"
#include "MotorControl.h"
#include "DetectionSystem.h"
#include "EventBus.h"
#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include "Logger.h"
#include "SafetyManager.h"

ConsoleCommands::ConsoleCommands(MotorControl &motor, DetectionSystem &detect, EventBus& eventBus, SignalControl& signalControl, SafetyManager &safety)
    : motor_(motor), detect_(detect), safety_(safety), eventBus_(eventBus), signalControl_(signalControl) {}

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
    motor_.setSimulationMode(true);
    detect_.setSimulationMode(true);
    safety_.setSimulationMode(true);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE ENABLED (motor + ultrasonic)");
    return true;
  }
  if (cmd == "sim off" || cmd == "simulation off")
  {
    motor_.setSimulationMode(false);
    detect_.setSimulationMode(false);
    safety_.setSimulationMode(false);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE DISABLED (motor + ultrasonic)");
    auto* resetData = new SimpleEventData(BridgeEvent::SYSTEM_RESET_REQUESTED);
    eventBus_.publish(BridgeEvent::SYSTEM_RESET_REQUESTED, resetData, EventPriority::EMERGENCY);
    LOG_INFO(Logger::TAG_CON, "System reset requested after exiting simulation mode");
    return true;
  }

  if (cmd == "test fault")
  {
    if (!safety_.isTestFaultActive())
    {
      safety_.triggerTestFault();
      LOG_WARN(Logger::TAG_CON, "TEST FAULT triggered. System entering emergency mode.");
    }
    else
    {
      LOG_INFO(Logger::TAG_CON, "TEST FAULT already active.");
    }
    return true;
  }

  if (cmd == "test clear" || cmd == "test off")
  {
    if (safety_.isTestFaultActive())
    {
      safety_.clearTestFault();
      LOG_INFO(Logger::TAG_CON, "TEST FAULT cleared. System back to normal operation.");
    }
    else
    {
      LOG_INFO(Logger::TAG_CON, "No TEST FAULT is currently active.");
    }
    return true;
  }

  if (cmd == "test status")
  {
    LOG_INFO(Logger::TAG_CON, "TEST FAULT STATUS: %s", safety_.isTestFaultActive() ? "ACTIVE" : "INACTIVE");
    return true;
  }

  if (!safety_.isSimulationMode() && safety_.isEmergencyActive())
  {
    LOG_WARN(Logger::TAG_CON, "System is in EMERGENCY mode. Use 'test clear' or 'test status'.");
    return true;
  }
  
  // Motor commands (mirroring existing strings)
  if (cmd == "raise" || cmd == "r") { motor_.raiseBridge(); return true; }
  if (cmd == "lower" || cmd == "l") { motor_.lowerBridge(); return true; }
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
  if (cmd == "test boat pass left" || cmd == "tbpl") {
    auto* eventData = new BoatEventData(BridgeEvent::BOAT_PASSED_LEFT, BoatEventSide::LEFT);
    eventBus_.publish(BridgeEvent::BOAT_PASSED_LEFT, eventData);
    LOG_INFO(Logger::TAG_CON, "TEST: Simulated boat passing through LEFT side");
    return true;
  }
  if (cmd == "test boat pass right" || cmd == "tbpr") {
    auto* eventData = new BoatEventData(BridgeEvent::BOAT_PASSED_RIGHT, BoatEventSide::RIGHT);
    eventBus_.publish(BridgeEvent::BOAT_PASSED_RIGHT, eventData);
    LOG_INFO(Logger::TAG_CON, "TEST: Simulated boat passing through RIGHT side");
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
  Serial.println("  test boat left|tbl       - Simulate boat from LEFT (exits RIGHT)");
  Serial.println("  test boat right|tbr      - Simulate boat from RIGHT (exits LEFT)");
  Serial.println("  test boat pass left|tbpl - Simulate boat exiting LEFT side");
  Serial.println("  test boat pass right|tbpr - Simulate boat exiting RIGHT side");
  Serial.println("  test fault               - Trigger manual test fault/emergency");
  Serial.println("  test clear|test off      - Clear manual test fault");
  Serial.println("  test status              - Show manual test fault status");
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
}

void ConsoleCommands::handleStreaming()
{
  const unsigned long now = millis();
  if (streamMask_ == 0)
    return;
  if (now - lastStreamMs_ < streamIntervalMs_)
    return;
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
}
