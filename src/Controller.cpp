#include "Controller.h"
#include <Arduino.h>

Controller::Controller(EventBus& eventBus, MotorControl& motorControl)
    : m_eventBus(eventBus), m_motorControl(motorControl) {
}

void Controller::handleCommand(const Command& command) {
    switch (command.target) {
        case CommandTarget::MOTOR_CONTROL:
            switch (command.action) {
                case CommandAction::RAISE_BRIDGE:
                    m_motorControl.raiseBridge();
                    break;
                case CommandAction::LOWER_BRIDGE:
                    m_motorControl.lowerBridge();
                    break;
                default:
                    Serial.println("CONTROLLER: Unknown action for MOTOR_CONTROL");
                    break;
            }
            break;

        case CommandTarget::SIGNAL_CONTROL:
            switch (command.action) {
                case CommandAction::STOP_TRAFFIC:
                    Serial.println("CONTROLLER: Command for SignalControl STOP_TRAFFIC received");
                    // TODO: Implement call
                    // m_signalControl.stopTraffic();
                    break;
                case CommandAction::RESUME_TRAFFIC:
                    Serial.println("CONTROLLER: Command for SignalControl RESUME_TRAFFIC received");
                    // TODO: Implement call
                    // m_signalControl.resumeTraffic();
                    break;
                default:
                    Serial.println("CONTROLLER: Unknown action for SIGNAL_CONTROL");
                    break;
            }
            break;

        case CommandTarget::LOCAL_STATE_INDICATOR:
            switch (command.action) {
                case CommandAction::SET_STATE:
                    Serial.println("CONTROLLER: Command for LocalStateIndicator SET_STATE received");
                    // TODO: Implement call
                    // m_localStateIndicator.setState();
                    break;
                default:
                    Serial.println("CONTROLLER: Unknown action for LOCAL_STATE_INDICATOR");
                    break;
            }
            break;

        case CommandTarget::CONTROLLER:
            if (command.action == CommandAction::ENTER_SAFE_STATE) {
                Serial.println("CONTROLLER: ENTER_SAFE_STATE command received.");
                m_motorControl.halt();
                // TODO: Call halt() on all other subsystems when they are implemented
                // e.g.,
                // m_signalControl.halt();
                // m_localStateIndicator.halt();
                m_eventBus.publish(BridgeEvent::SYSTEM_SAFE_SUCCESS);
            } else {
                Serial.println("CONTROLLER: Unknown action for CONTROLLER");
            }
            break;

        default:
            Serial.println("CONTROLLER: Unknown command target");
            break;
    }
}