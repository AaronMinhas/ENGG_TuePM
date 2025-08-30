#include "Controller.h"
#include <Arduino.h>

Controller::Controller(EventBus& eventBus, CommandBus& commandBus, MotorControl& motorControl,
                       SignalControl& signalControl, LocalStateIndicator& localStateIndicator)
    : m_eventBus(eventBus), 
      m_commandBus(commandBus),
      m_motorControl(motorControl),
      m_signalControl(signalControl),
      m_localStateIndicator(localStateIndicator) {
}

void Controller::begin() {
    subscribeToCommands();
    Serial.println("CONTROLLER: Initialised and subscribed to CommandBus");
}

void Controller::subscribeToCommands() {
    // Subscribe to all command targets that this controller handles
    auto commandCallback = [this](const Command& command) {
        this->handleCommand(command);
    };

    m_commandBus.subscribe(CommandTarget::CONTROLLER, commandCallback);
    m_commandBus.subscribe(CommandTarget::MOTOR_CONTROL, commandCallback);
    m_commandBus.subscribe(CommandTarget::SIGNAL_CONTROL, commandCallback);
    m_commandBus.subscribe(CommandTarget::LOCAL_STATE_INDICATOR, commandCallback);
    
    Serial.println("CONTROLLER: Subscribed to all command targets on CommandBus");
}

void Controller::handleCommand(const Command& command) {
    Serial.print("CONTROLLER: Processing command - Target: ");
    Serial.print((int)command.target);
    Serial.print(", Action: ");
    Serial.println((int)command.action);

    switch (command.target) {
        case CommandTarget::MOTOR_CONTROL:
            switch (command.action) {
                case CommandAction::RAISE_BRIDGE:
                    Serial.println("CONTROLLER: Calling MotorControl::raiseBridge()");
                    m_motorControl.raiseBridge();
                    break;
                case CommandAction::LOWER_BRIDGE:
                    Serial.println("CONTROLLER: Calling MotorControl::lowerBridge()");
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
                    Serial.println("CONTROLLER: Calling SignalControl::stopTraffic()");
                    m_signalControl.stopTraffic();
                    break;
                case CommandAction::RESUME_TRAFFIC:
                    Serial.println("CONTROLLER: Calling SignalControl::resumeTraffic()");
                    m_signalControl.resumeTraffic();
                    break;
                default:
                    Serial.println("CONTROLLER: Unknown action for SIGNAL_CONTROL");
                    break;
            }
            break;

        case CommandTarget::LOCAL_STATE_INDICATOR:
            switch (command.action) {
                case CommandAction::SET_STATE:
                    Serial.println("CONTROLLER: Calling LocalStateIndicator::setState()");
                    m_localStateIndicator.setState();
                    break;
                default:
                    Serial.println("CONTROLLER: Unknown action for LOCAL_STATE_INDICATOR");
                    break;
            }
            break;

        case CommandTarget::CONTROLLER:
            if (command.action == CommandAction::ENTER_SAFE_STATE) {
                Serial.println("CONTROLLER: ENTER_SAFE_STATE command received - halting all subsystems");
                
                // Halt all subsystems
                m_motorControl.halt();
                m_signalControl.halt();
                m_localStateIndicator.halt();
                
                Serial.println("CONTROLLER: All subsystems halted - system in safe state");
                m_eventBus.publish(BridgeEvent::SYSTEM_SAFE_SUCCESS);
            } else {
                Serial.println("CONTROLLER: Unknown action for CONTROLLER");
            }
            break;

        default:
            Serial.print("CONTROLLER: Unknown command target: ");
            Serial.println((int)command.target);
            break;
    }
}