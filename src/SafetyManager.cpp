#include "SafetyManager.h"
#include "Logger.h"
#include "MotorControl.h"
#include "SignalControl.h"
#include <Arduino.h>

bool testFaultActive = false;

SafetyManager::SafetyManager(EventBus &eventBus, CommandBus &commandBus)
    : m_eventBus(eventBus), m_commandBus(commandBus), m_motorControl(nullptr), m_signalControl(nullptr), m_emergencyActive(false), m_simulationMode(true) // Start in simulation mode by default
      ,
      m_lastStateEvent(BridgeEvent::FAULT_CLEARED) // Safe initial value
      ,
      m_stateEventTime(0), m_lastFaultReason("")
{
}

void SafetyManager::begin()
{
    LOG_INFO(Logger::TAG_SYS, "Initializing Safety Manager...");

    // Subscribe to all relevant events - must specify event type
    m_eventBus.subscribe(BridgeEvent::BOAT_DETECTED, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::TRAFFIC_STOPPED_SUCCESS, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::BRIDGE_OPENED_SUCCESS, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::BOAT_PASSED, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::BRIDGE_CLOSED_SUCCESS, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::TRAFFIC_RESUMED_SUCCESS, [this](EventData *data)
                         { this->onEvent(data); });
    m_eventBus.subscribe(BridgeEvent::FAULT_DETECTED, [this](EventData *data)
                         { this->onEvent(data); });

    // Subscribe to commands targeted for safety manager
    m_commandBus.subscribe(CommandTarget::SAFETY_MANAGER, [this](const Command &command)
                           { this->handleCommand(command); });

    LOG_INFO(Logger::TAG_SYS, "Safety Manager initialized successfully");
}

void SafetyManager::update()
{
    // Skip safety checks in simulation mode
    if (m_simulationMode)
    {
        return;
    }

    // Check state transition timeouts
    checkStateTransitionTimeouts();
}

void SafetyManager::triggerEmergency(const char *reason)
{
    // std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_emergencyActive)
    {
        m_emergencyActive = true;
        LOG_ERROR(Logger::TAG_SYS, "EMERGENCY ACTIVATED: %s", reason);

        enterSafeState(reason);
    }
}

bool SafetyManager::isEmergencyActive() const
{
    return m_emergencyActive;
}

void SafetyManager::clearEmergency()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_emergencyActive)
    {
        LOG_INFO(Logger::TAG_SYS, "Emergency cleared");
        m_emergencyActive = false;
    }
}

void SafetyManager::setMotorControl(MotorControl *motorControl)
{
    m_motorControl = motorControl;
}

void SafetyManager::setSignalControl(SignalControl *signalControl)
{
    m_signalControl = signalControl;
}

void SafetyManager::setSimulationMode(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_simulationMode = enabled;
    LOG_INFO(Logger::TAG_SYS, "Safety Manager simulation mode %s",
             enabled ? "enabled" : "disabled");
}

bool SafetyManager::isSimulationMode() const
{
    return m_simulationMode;
}

// Private methods
void SafetyManager::enterSafeState(const char *reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_lastFaultReason = String(reason);

    LOG_ERROR(Logger::TAG_SYS, "ENTERING SAFE STATE: %s", reason);

    // Direct hardware control - stop motor immediately
    if (m_motorControl != nullptr)
    {
        m_motorControl->halt(); // Use halt() instead of emergencyStop()
        LOG_INFO(Logger::TAG_SYS, "Motor emergency stop triggered");
    }
    else
    {
        LOG_ERROR(Logger::TAG_SYS, "Cannot stop motor - MotorControl reference is null");
    }

    // Direct hardware control - set all lights to red
    if (m_signalControl != nullptr)
    {
        m_signalControl->halt();
        LOG_INFO(Logger::TAG_SYS, "Traffic signals set to emergency state");
    }
    else
    {
        LOG_ERROR(Logger::TAG_SYS, "Cannot control signals - SignalControl reference is null");
    }

    // Publish fault event with high priority
    EventData *faultData = new SimpleEventData(BridgeEvent::FAULT_DETECTED);
    m_eventBus.publish(BridgeEvent::FAULT_DETECTED, faultData, EventPriority::EMERGENCY);
}

void SafetyManager::checkStateTransitionTimeouts()
{
    // Skip if no valid state event recorded or in emergency already
    if (m_stateEventTime == 0 || m_emergencyActive)
    {
        return;
    }

    unsigned long elapsed = millis() - m_stateEventTime;
    unsigned long timeout = getStateTimeout(m_lastStateEvent);

    // If timeout occurred
    if (elapsed > timeout)
    {
        BridgeEvent expectedNext = getExpectedNextEvent(m_lastStateEvent);

        // Build error message without String concatenation issues
        char reason[200];
        snprintf(reason, sizeof(reason), "State transition timeout: Event %s did not transition to %s within %lums",
                 bridgeEventToString(m_lastStateEvent),
                 bridgeEventToString(expectedNext),
                 timeout);

        LOG_ERROR(Logger::TAG_SYS, "%s", reason);
        triggerEmergency(reason);
        m_stateEventTime = 0; // Reset timer
    }
}

void SafetyManager::onEvent(EventData *data)
{
    BridgeEvent event = data->getEventEnum();

    // Check for events we need to monitor for timeouts
    switch (event)
    {
    case BridgeEvent::BOAT_DETECTED:
        // Start monitoring for expected next state
        m_lastStateEvent = event;
        m_stateEventTime = millis();
        LOG_INFO(Logger::TAG_SYS, "Monitoring state transition from %s (timeout: %lums)",
                 bridgeEventToString(event), getStateTimeout(event));
        break;

    case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
        // If monitoring from BOAT_DETECTED, clear timer
        if (m_lastStateEvent == BridgeEvent::BOAT_DETECTED)
        {
            m_stateEventTime = 0; // Clear timer
        }
        else
        {
            // Otherwise, start monitoring for next state
            m_lastStateEvent = event;
            m_stateEventTime = millis();
            LOG_INFO(Logger::TAG_SYS, "Monitoring state transition from %s (timeout: %lums)",
                     bridgeEventToString(event), getStateTimeout(event));
        }
        break;

    case BridgeEvent::BRIDGE_OPENED_SUCCESS:
        // If monitoring from TRAFFIC_STOPPED_SUCCESS, clear timer
        if (m_lastStateEvent == BridgeEvent::TRAFFIC_STOPPED_SUCCESS)
        {
            m_stateEventTime = 0; // Clear timer
        }
        else
        {
            // Otherwise, start monitoring for next state
            m_lastStateEvent = event;
            m_stateEventTime = millis();
            LOG_INFO(Logger::TAG_SYS, "Monitoring state transition from %s (timeout: %lums)",
                     bridgeEventToString(event), getStateTimeout(event));
        }
        break;

    case BridgeEvent::BOAT_PASSED:
        // If monitoring from BRIDGE_OPENED_SUCCESS, clear timer
        if (m_lastStateEvent == BridgeEvent::BRIDGE_OPENED_SUCCESS)
        {
            m_stateEventTime = 0; // Clear timer
        }
        else
        {
            // Otherwise, start monitoring for next state
            m_lastStateEvent = event;
            m_stateEventTime = millis();
            LOG_INFO(Logger::TAG_SYS, "Monitoring state transition from %s (timeout: %lums)",
                     bridgeEventToString(event), getStateTimeout(event));
        }
        break;

    case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
        // If monitoring from BOAT_PASSED, clear timer
        if (m_lastStateEvent == BridgeEvent::BOAT_PASSED)
        {
            m_stateEventTime = 0; // Clear timer
        }
        else
        {
            // Otherwise, start monitoring for next state
            m_lastStateEvent = event;
            m_stateEventTime = millis();
            LOG_INFO(Logger::TAG_SYS, "Monitoring state transition from %s (timeout: %lums)",
                     bridgeEventToString(event), getStateTimeout(event));
        }
        break;

    case BridgeEvent::TRAFFIC_RESUMED_SUCCESS:
        // If monitoring from BRIDGE_CLOSED_SUCCESS, clear timer
        if (m_lastStateEvent == BridgeEvent::BRIDGE_CLOSED_SUCCESS)
        {
            m_stateEventTime = 0; // Clear timer
        }
        break;

    case BridgeEvent::FAULT_DETECTED:
        // Requirement 6: If FAULT_DETECTED, trigger emergency
        triggerEmergency("Fault detected by system");
        break;

    default:
        // Most events don't need special handling
        break;
    }
}

void SafetyManager::handleCommand(const Command &command)
{
    if (command.action == CommandAction::ENTER_SAFE_STATE)
    {
        triggerEmergency("Remote emergency trigger");
    }
    else
    {
        LOG_WARN(Logger::TAG_SYS, "SafetyManager received unknown command: %d",
                 static_cast<int>(command.action));
    }
}

BridgeEvent SafetyManager::getExpectedNextEvent(BridgeEvent currentEvent)
{
    switch (currentEvent)
    {
    case BridgeEvent::BOAT_DETECTED:
        return BridgeEvent::TRAFFIC_STOPPED_SUCCESS;

    case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
        return BridgeEvent::BRIDGE_OPENED_SUCCESS;

    case BridgeEvent::BRIDGE_OPENED_SUCCESS:
        return BridgeEvent::BOAT_PASSED;

    case BridgeEvent::BOAT_PASSED:
        return BridgeEvent::BRIDGE_CLOSED_SUCCESS;

    case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
        return BridgeEvent::TRAFFIC_RESUMED_SUCCESS;

    default:
        return BridgeEvent::FAULT_DETECTED; // Default safety value
    }
}

unsigned long SafetyManager::getStateTimeout(BridgeEvent stateEvent)
{
    switch (stateEvent)
    {
    case BridgeEvent::BOAT_DETECTED:
        return BOAT_DETECTED_TIMEOUT_MS;

    case BridgeEvent::TRAFFIC_STOPPED_SUCCESS:
        return TRAFFIC_STOPPED_TIMEOUT_MS;

    case BridgeEvent::BRIDGE_OPENED_SUCCESS:
        return BRIDGE_OPENED_TIMEOUT_MS;

    case BridgeEvent::BOAT_PASSED:
        return BOAT_PASSED_TIMEOUT_MS;

    case BridgeEvent::BRIDGE_CLOSED_SUCCESS:
        return BRIDGE_CLOSED_TIMEOUT_MS;

    default:
        return 5000; // Default 5-second timeout for unspecified states
    }
}

void SafetyManager::triggerTestFault()
{
    if (!testFaultActive)
    {
        testFaultActive = true;
        triggerEmergency("Manual TEST FAULT triggered via console");
    }
}

void SafetyManager::clearTestFault()
{
    if (!testFaultActive)
    {
        LOG_INFO(Logger::TAG_SAFE, "clearTestFault() called but no TEST FAULT is active.");
        return;
    }

    testFaultActive = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_emergencyActive = false;
    }

    LOG_INFO(Logger::TAG_SAFE, "TEST FAULT cleared. Restoring normal operations.");

    if (m_motorControl != nullptr)
    {
        m_motorControl->halt();
    }
    else
    {
        LOG_WARN(Logger::TAG_SAFE, "MotorControl reference null during fault clear - motor state unchanged.");
    }

    if (m_signalControl != nullptr)
    {
        m_signalControl->setCarTraffic("Green");
        m_signalControl->setBoatLight("left", "Red");
        m_signalControl->setBoatLight("right", "Red");
    }
    else
    {
        LOG_WARN(Logger::TAG_SAFE, "SignalControl reference null during fault clear - traffic lights unchanged.");
    }

    auto *faultCleared = new SimpleEventData(BridgeEvent::FAULT_CLEARED);
    m_eventBus.publish(BridgeEvent::FAULT_CLEARED, faultCleared, EventPriority::EMERGENCY);
}

bool SafetyManager::isTestFaultActive() const
{
    return testFaultActive;
}
