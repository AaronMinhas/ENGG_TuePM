#include "CommandBus.h"

// Global instance
CommandBus commandBus;

CommandBus::CommandBus() {
    // Initialize command bus
    // Initialization code here if needed
}

CommandBus::~CommandBus() {
    // Acquire lock before modifying shared data structure
    std::lock_guard<std::mutex> lock(bus_mutex);

    // There's no specific resource to release, so this is just a safety step.
    // If subscribers need explicit cleanup, you can iterate and call delete for each one.
    subscribers.clear();  // Clear all subscriptions (if required)
}

void CommandBus::publish(const Command& command) {
    // Lock the subscribers map for thread safety
    std::lock_guard<std::mutex> lock(bus_mutex);

    // Find subscribers for the command's target
    auto it = subscribers.find(command.target);
    if (it != subscribers.end()) {
        // Only log for motor control commands (most important)
        if (command.target == CommandTarget::MOTOR_CONTROL) {
            Serial.printf("COMMAND: Motor.%s\n", 
                (command.action == CommandAction::RAISE_BRIDGE) ? "raise()" : 
                (command.action == CommandAction::LOWER_BRIDGE) ? "lower()" : "halt()");
        }
        
        // Make a local copy of callbacks to minimize lock duration
        auto callbacks = it->second;
        // Call each subscriber's callback with the command
        for (const auto& callback : callbacks) {
            if (callback) {
                callback(command);
            }
        }
    }
}

void CommandBus::subscribe(CommandTarget target, std::function<void(const Command&)> callback) {
    // Lock the subscribers map for thread safety
    std::lock_guard<std::mutex> lock(bus_mutex);

    // Add the callback to the list of subscribers for the target
    subscribers[target].push_back(callback);
}

void CommandBus::unsubscribe(CommandTarget target) {
    // Lock the subscribers map for thread safety
    std::lock_guard<std::mutex> lock(bus_mutex);

    // Remove all subscriptions for the target
    subscribers.erase(target);
}

bool CommandBus::hasSubscribers(CommandTarget target) const {
   std::lock_guard<std::mutex> lock(bus_mutex);

    auto it = subscribers.find(target);
    return (it != subscribers.end() && !it->second.empty());
}

void CommandBus::clear() {
   std::lock_guard<std::mutex> lock(bus_mutex);

    // Remove all subscriptions from all targets
    subscribers.clear();
}