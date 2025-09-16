#include "ConsoleCommands.h"
#include "MotorControl.h"
#include "DetectionSystem.h"

ConsoleCommands::ConsoleCommands(MotorControl& motor, DetectionSystem& detect)
  : motor_(motor), detect_(detect) {}

void ConsoleCommands::begin() {
  Serial.println("CONSOLE: Commands ready. Type 'help' for options.");
}

void ConsoleCommands::poll() {
  // Always allow streaming even if no new input arrived
  handleStreaming();
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  String lower = cmd; lower.toLowerCase();
  Serial.printf("CONSOLE: Received: '%s'\n", lower.c_str());
  handleCommand(lower);
  // After handling any command, also process streaming if enabled
  handleStreaming();
}

void ConsoleCommands::handleCommand(const String& cmd) {
  // Global simulation toggles
  if (cmd == "sim on" || cmd == "simulation on") {
    motor_.setSimulationMode(true);
    detect_.setSimulationMode(true);
    Serial.println("CONSOLE: SIMULATION MODE ENABLED (motor + ultrasonic)");
    return;
  }
  if (cmd == "sim off" || cmd == "simulation off") {
    motor_.setSimulationMode(false);
    detect_.setSimulationMode(false);
    Serial.println("CONSOLE: SIMULATION MODE DISABLED (motor + ultrasonic)");
    return;
  }

  // Motor commands (mirroring existing strings)
  if (cmd == "raise" || cmd == "r") { motor_.raiseBridge(); return; }
  if (cmd == "lower" || cmd == "l") { motor_.lowerBridge(); return; }
  if (cmd == "halt"  || cmd == "h" || cmd == "stop") { motor_.halt(); return; }
  if (cmd == "test motor" || cmd == "tm") { motor_.testMotor(); return; }
  if (cmd == "test encoder" || cmd == "te") { motor_.testEncoder(); return; }
  if (cmd == "count" || cmd == "c") { Serial.printf("MOTOR CONTROL: Current encoder count: %ld\n", motor_.getEncoderCount()); return; }
  if (cmd == "reset" || cmd == "0") { motor_.resetEncoder(); return; }

  // Ultrasonic/detection commands
  // Short toggle: 'us' toggles streaming on/off; 'us state' prints status
  if (cmd == "us") {
    streamEnabled_ = !streamEnabled_;
    Serial.printf("ULTRA STREAM: %s\n", streamEnabled_ ? "enabled" : "disabled");
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

  if (cmd == "ultra stream on") { streamEnabled_ = true; Serial.println("ULTRA STREAM: enabled"); return; }
  if (cmd == "ultra stream off") { streamEnabled_ = false; Serial.println("ULTRA STREAM: disabled"); return; }
  if (cmd.startsWith("ultra stream ")) {
    String rest = cmd.substring(String("ultra stream ").length());
    rest.trim();
    // Accept plain milliseconds (e.g., "100")
    long val = rest.toInt();
    if (val > 0) {
      streamIntervalMs_ = (unsigned long)val;
      Serial.printf("ULTRA STREAM: interval set to %ld ms\n", val);
      return;
    }
  }

  if (cmd == "status" || cmd == "mode") { printStatus(); return; }
  if (cmd == "help" || cmd == "?") { printHelp(); return; }

  Serial.println("CONSOLE: Unknown command. Type 'help' for available commands.");
}

void ConsoleCommands::printHelp() {
  Serial.println("Available commands:");
  Serial.println("  sim on / simulation on    - Enable simulation (motor + ultrasonic)");
  Serial.println("  sim off / simulation off  - Disable simulation (motor + ultrasonic)");
  Serial.println("  raise|r / lower|l / halt|h / stop");
  Serial.println("  test motor|tm / test encoder|te / count|c / reset|0");
  Serial.println("  us                        - Toggle ultrasonic streaming on/off");
  Serial.println("  us state                  - Show ultrasonic status (distance, zone, sim)");
  Serial.println("  ultra status              - Show ultrasonic status (same as 'us state')");
  Serial.println("  ultra read|ur             - Take a reading tick and show status");
  Serial.println("  ultra stream on|off       - Start/stop streaming readings (no timestamps)");
  Serial.println("  ultra stream <ms>         - Set stream interval (e.g., 100 for 10Hz)");
  Serial.println("  status|mode               - Show combined status");
  Serial.println("  help|?                    - Show this help");
}

void ConsoleCommands::printStatus() {
  Serial.printf("MOTOR CONTROL: Mode: %s\n", motor_.isSimulationMode() ? "SIMULATION" : "REAL");
  const float d = detect_.getFilteredDistanceCm();
  Serial.printf("ULTRASONIC: Mode: %s, distance: %s, zone: %s\n",
                detect_.isSimulationMode() ? "SIM" : "REAL",
                (d > 0 ? String(d, 1).c_str() : "unknown"),
                detect_.zoneName());
}

void ConsoleCommands::handleStreaming() {
  const unsigned long now = millis();
  if (!streamEnabled_) return;
  if (now - lastStreamMs_ < streamIntervalMs_) return;
  lastStreamMs_ = now;

  // Ensure sensor gets a chance to read
  detect_.update();
  const float d = detect_.getFilteredDistanceCm();
  Serial.printf("ULTRASONIC: dist=%s cm, zone=%s, mode=%s\n",
                (d > 0 ? String(d, 1).c_str() : "unknown"),
                detect_.zoneName(),
                detect_.isSimulationMode() ? "SIM" : "REAL");
}
