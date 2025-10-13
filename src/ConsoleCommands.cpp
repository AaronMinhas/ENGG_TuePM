#include "ConsoleCommands.h"
#include "MotorControl.h"
#include "DetectionSystem.h"
#include "EventBus.h"
#include "SignalControl.h"
#include "BridgeSystemDefs.h"
#include "Logger.h"

ConsoleCommands::ConsoleCommands(MotorControl& motor, DetectionSystem& detect, EventBus& eventBus, SignalControl& signalControl)
  : motor_(motor), detect_(detect), eventBus_(eventBus), signalControl_(signalControl) {}

void ConsoleCommands::begin() {
  LOG_INFO(Logger::TAG_CON, "Commands ready. Type 'help' for options.");
}

void ConsoleCommands::poll() {
  // Always allow streaming even if no new input arrived
  handleStreaming();
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  String lower = cmd; lower.toLowerCase();
  LOG_DEBUG(Logger::TAG_CON, "Received: '%s'", lower.c_str());
  handleCommand(lower);
  // After handling any command, also process streaming if enabled
  handleStreaming();
}

void ConsoleCommands::handleCommand(const String& cmd) {
  // Global simulation toggles
  if (cmd == "sim on" || cmd == "simulation on") {
    motor_.setSimulationMode(true);
    detect_.setSimulationMode(true);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE ENABLED (motor + ultrasonic)");
    return;
  }
  if (cmd == "sim off" || cmd == "simulation off") {
    motor_.setSimulationMode(false);
    detect_.setSimulationMode(false);
    LOG_INFO(Logger::TAG_CON, "SIMULATION MODE DISABLED (motor + ultrasonic)");
    return;
  }

  // Motor commands (mirroring existing strings)
  if (cmd == "raise" || cmd == "r") { motor_.raiseBridge(); return; }
  if (cmd == "lower" || cmd == "l") { motor_.lowerBridge(); return; }
  if (cmd == "halt"  || cmd == "h" || cmd == "stop") { motor_.halt(); return; }
  if (cmd == "test motor" || cmd == "tm") { motor_.testMotor(); return; }
  if (cmd == "test encoder" || cmd == "te") { motor_.testEncoder(); return; }
  if (cmd == "count" || cmd == "c") { LOG_INFO(Logger::TAG_MC, "MOTOR CONTROL: Current encoder count: %ld", motor_.getEncoderCount()); return; }
  if (cmd == "reset" || cmd == "0") { motor_.resetEncoder(); return; }

  // Fake boat event commands
  if (cmd == "fake boat detected" || cmd == "fbd") {
    eventBus_.publish(BridgeEvent::BOAT_DETECTED);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_DETECTED published");
    return;
  }
  if (cmd == "fake boat passed" || cmd == "fbp") {
    eventBus_.publish(BridgeEvent::BOAT_PASSED);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_PASSED published");
    return;
  }
  if (cmd == "fake boat left" || cmd == "fbl") {
    eventBus_.publish(BridgeEvent::BOAT_DETECTED_LEFT);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_DETECTED_LEFT published");
    return;
  }
  if (cmd == "fake boat right" || cmd == "fbr") {
    eventBus_.publish(BridgeEvent::BOAT_DETECTED_RIGHT);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_DETECTED_RIGHT published");
    return;
  }
  if (cmd == "fake boat passed left" || cmd == "fbpl") {
    eventBus_.publish(BridgeEvent::BOAT_PASSED_LEFT);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_PASSED_LEFT published");
    return;
  }
  if (cmd == "fake boat passed right" || cmd == "fbpr") {
    eventBus_.publish(BridgeEvent::BOAT_PASSED_RIGHT);
    LOG_INFO(Logger::TAG_CON, "FAKE EVENT: BOAT_PASSED_RIGHT published");
    return;
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
    return;
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
    return;
  }
  
  if (cmd == "lights status" || cmd == "ls") {
    LOG_INFO(Logger::TAG_CON, "LIGHT CONTROL: Use 'status' command to see current system state");
    return;
  }

  // Ultrasonic/detection commands
  // Short toggle: 'us' toggles streaming (both sensors); 'us state' prints status
  if (cmd == "us") {
    const bool enable = streamMask_ != (STREAM_LEFT | STREAM_RIGHT);
    streamMask_ = enable ? (STREAM_LEFT | STREAM_RIGHT) : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (both sensors)", enable ? "enabled" : "disabled");
    return;
  }
  if (cmd == "usl") {
    const bool enable = streamMask_ != STREAM_LEFT;
    streamMask_ = enable ? STREAM_LEFT : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (left sensor)", enable ? "enabled" : "disabled");
    return;
  }
  if (cmd == "usr") {
    const bool enable = streamMask_ != STREAM_RIGHT;
    streamMask_ = enable ? STREAM_RIGHT : 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: %s (right sensor)", enable ? "enabled" : "disabled");
    return;
  }
  if (cmd == "us state") { printStatus(); return; }
  if (cmd == "ultra status") { printStatus(); return; }
  if (cmd == "ultra read" || cmd == "ur") {
    // Force an update tick and then print status
    detect_.update();
    printStatus();
    return;
  }

  if (cmd.startsWith("log level ")) {
    String levelStr = cmd.substring(String("log level ").length());
    levelStr.trim();
    levelStr.toLowerCase();
    Logger::Level newLevel = Logger::Level::INFO;
    bool matched = true;
    if (levelStr == "debug") newLevel = Logger::Level::DEBUG;
    else if (levelStr == "info") newLevel = Logger::Level::INFO;
    else if (levelStr == "warn" || levelStr == "warning") newLevel = Logger::Level::WARN;
    else if (levelStr == "error") newLevel = Logger::Level::ERROR;
    else if (levelStr == "none") newLevel = Logger::Level::NONE;
    else matched = false;

    if (matched) {
      Logger::setLevel(newLevel);
      LOG_INFO(Logger::TAG_CON, "Log level set to %s", Logger::levelToString(newLevel));
    } else {
      LOG_WARN(Logger::TAG_CON, "Unknown log level '%s'", levelStr.c_str());
    }
    return;
  }
  
  // Individual sensor commands for debugging
  if (cmd == "ultra left" || cmd == "ul") {
    detect_.update();
    const float leftDist = detect_.getLeftFilteredDistanceCm();
    LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_LEFT: dist=%s cm, zone=%s, mode=%s",
              (leftDist > 0 ? String(leftDist, 1).c_str() : "unknown"),
              detect_.getLeftZoneName(),
              detect_.isSimulationMode() ? "SIM" : "REAL");
    return;
  }
  if (cmd == "ultra right" || cmd == "ur2") {
    detect_.update();
    const float rightDist = detect_.getRightFilteredDistanceCm();
    LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_RIGHT: dist=%s cm, zone=%s, mode=%s",
              (rightDist > 0 ? String(rightDist, 1).c_str() : "unknown"),
              detect_.getRightZoneName(),
              detect_.isSimulationMode() ? "SIM" : "REAL");
    return;
  }

  if (cmd == "ultra stream on") {
    streamMask_ = STREAM_LEFT | STREAM_RIGHT;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: enabled (both sensors)");
    return;
  }
  if (cmd == "ultra stream off") {
    streamMask_ = 0;
    LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: disabled");
    return;
  }
  if (cmd.startsWith("ultra stream ")) {
    String rest = cmd.substring(String("ultra stream ").length());
    rest.trim();
    // Accept plain milliseconds (e.g., "100")
    long val = rest.toInt();
    if (val > 0) {
      streamIntervalMs_ = (unsigned long)val;
      LOG_INFO(Logger::TAG_DS, "ULTRA STREAM: interval set to %ld ms", val);
      return;
    }
  }

  if (cmd == "status" || cmd == "mode") { printStatus(); return; }
  if (cmd == "help" || cmd == "?") { printHelp(); return; }

  LOG_WARN(Logger::TAG_CON, "Unknown command. Type 'help' for available commands.");
}

void ConsoleCommands::printHelp() {
  Serial.println("Available commands:");
  Serial.println("  sim on / simulation on    - Enable simulation (motor + ultrasonic)");
  Serial.println("  sim off / simulation off  - Disable simulation (motor + ultrasonic)");
  Serial.println("  raise|r / lower|l / halt|h / stop");
  Serial.println("  test motor|tm / test encoder|te / count|c / reset|0");
  Serial.println("  us                        - Toggle ultrasonic streaming for both sensors");
  Serial.println("  usl                       - Toggle ultrasonic streaming for left sensor only");
  Serial.println("  usr                       - Toggle ultrasonic streaming for right sensor only");
  Serial.println("  us state                  - Show ultrasonic status (both sensors)");
  Serial.println("  ultra status              - Show ultrasonic status (same as 'us state')");
  Serial.println("  ultra read|ur             - Take a reading tick and show both sensors");
  Serial.println("  ultra left|ul             - Read left sensor only");
  Serial.println("  ultra right|ur2           - Read right sensor only");
  Serial.println("  ultra stream on|off       - Start/stop streaming readings (both sensors)");
  Serial.println("  ultra stream <ms>         - Set stream interval (e.g., 100 for 10Hz)");
  Serial.println("  fake boat detected|fbd   - Trigger fake BOAT_DETECTED event");
  Serial.println("  fake boat passed|fbp     - Trigger fake BOAT_PASSED event");
  Serial.println("  fake boat left|fbl       - Trigger fake BOAT_DETECTED_LEFT event");
  Serial.println("  fake boat right|fbr      - Trigger fake BOAT_DETECTED_RIGHT event");
  Serial.println("  fake boat passed left|fbpl  - Trigger fake BOAT_PASSED_LEFT event");
  Serial.println("  fake boat passed right|fbpr  - Trigger fake BOAT_PASSED_RIGHT event");
  Serial.println("  car light <colour>|cl <colour>  - Set car lights (red/yellow/green)");
  Serial.println("  boat light <side> <colour>|bl <side> <colour>  - Set boat lights (left/right, red/green)");
  Serial.println("  lights status|ls          - Show light control status");
  Serial.println("  log level <lvl>           - Set log level (debug/info/warn/error/none)");
  Serial.println("  status|mode               - Show combined status");
  Serial.println("  help|?                    - Show this help");
}

void ConsoleCommands::printStatus() {
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

void ConsoleCommands::handleStreaming() {
  const unsigned long now = millis();
  if (streamMask_ == 0) return;
  if (now - lastStreamMs_ < streamIntervalMs_) return;
  lastStreamMs_ = now;

  // Ensure sensor gets a chance to read
  detect_.update();

  if (streamMask_ & STREAM_LEFT) {
    const float leftDist = detect_.getLeftFilteredDistanceCm();
  LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_LEFT: dist=%s cm, zone=%s, mode=%s",
            (leftDist > 0 ? String(leftDist, 1).c_str() : "unknown"),
            detect_.getLeftZoneName(),
            detect_.isSimulationMode() ? "SIM" : "REAL");
  }

  if (streamMask_ & STREAM_RIGHT) {
    const float rightDist = detect_.getRightFilteredDistanceCm();
  LOG_DEBUG(Logger::TAG_DS, "ULTRASONIC_RIGHT: dist=%s cm, zone=%s, mode=%s",
            (rightDist > 0 ? String(rightDist, 1).c_str() : "unknown"),
            detect_.getRightZoneName(),
            detect_.isSimulationMode() ? "SIM" : "REAL");
  }
}
