#pragma once  // Header guard: ensures this file is included only once per compilation

#ifdef UNIT_TEST
// For unit testing, Arduino header is not needed.

#else
    #include <Arduino.h>
#endif

// Standard library includes (from C++ Standard Library namespace "std")
#include <map>             // For std::map (key-value storage, like HashMap in Java)
#include <vector>          // For std::vector (dynamic array, like ArrayList in Java)
#include <functional>      // For std::function (storing callbacks/function pointers)
#include <mutex>           // For std::mutex (thread synchronization)
#include "BridgeSystemDefs.h"  // Project-specific definitions (Command struct, etc.)

/**
 * CommandBus - Central command distribution system for outgoing instructions
 * 
 * Handles routing of commands from StateMachine to appropriate subsystems
 * Commands represent instructions for future actions (e.g., STOP_TRAFFIC)
 * 
 * Uses publish-subscribe pattern where:
 * - StateMachine publishes commands (e.g., OPEN_BRIDGE)
 * - Subsystems (Motor, Signal) subscribe to receive specific commands
 * 
 * Thread-safe implementation with mutex protection for multi-threaded environments
 */
class CommandBus {
public:
    // Constructor & Destructor
    CommandBus();  // Initialize command bus
    ~CommandBus(); // Clean up resources when object is destroyed
    
    // Core functionality methods
    
    // Sends a command to all subscribed handlers for the command's target
    // const parameter means command cannot be modified inside the function
    // Thread-safe: protected by mutex
    void publish(const Command& command);
    
    // Registers a callback function to be called when commands for specified target are published
    // std::function is a wrapper that can store any callable object (function pointer, lambda, etc.)
    // Thread-safe: protected by mutex
    void subscribe(CommandTarget target, std::function<void(const Command&)> callback);
    
    // Removes all callbacks for the specified target
    // Thread-safe: protected by mutex
    void unsubscribe(CommandTarget target);
    
    // Utility methods
    
    // Checks if any callbacks are registered for the target
    // const after function means this function doesn't modify the object's state
    // Thread-safe: protected by mutex
    bool hasSubscribers(CommandTarget target) const;
    
    // Removes all subscriptions
    // Thread-safe: protected by mutex
    void clear();

private:
    // Maps command targets to their list of callback functions
    // Example: MOTOR -> [openBridgeFunction, logCommandFunction]
    // Each vector can grow dynamically as new callbacks are added
    std::map<CommandTarget, std::vector<std::function<void(const Command&)>>> subscribers;
    
    // Mutex for thread synchronization
    // Protects access to the subscribers map during concurrent operations
    mutable std::mutex bus_mutex; // mutable allows modification in const methods
};

// Global instance declaration
// extern defined (in CommandBus.cpp)
// Allows all files to access the same CommandBus instance
extern CommandBus commandBus;