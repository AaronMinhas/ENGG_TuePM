#pragma once

#include <Arduino.h>
#include <map>
#include <vector>
#include <functional>
#include <mutex>          
#include "BridgeSystemDefs.h"

// Forward declaration
class EventData;

/**
 * Priority levels for events
 * EMERGENCY events are processed before any NORMAL events
 * Used for safety-critical events that need immediate handling
 */
enum class EventPriority {
    NORMAL,    // Regular operational events
    EMERGENCY  // Safety-critical events that take precedence
};

/**
 * Base class for all event data
 * Used to pass additional information with events
 */
class EventData {
public:
    // Virtual destructor enables proper cleanup of derived classes
    virtual ~EventData();
    
    // Returns a string representation of the event type
    // 'virtual' allows derived classes to override this method
    // 'const' means this method doesn't modify the object
    virtual const char* getEventType() const;
    
    // Returns the enum value representing this event
    // Used for event type identification and routing
    virtual BridgeEvent getEventEnum() const;
};

/**
 * Represents a subscription to an event type
 * Contains both the callback function and its priority
 */
struct EventSubscription {
    std::function<void(EventData*)> callback;  // Function to call when event occurs
    EventPriority priority;                    // Priority of this subscription
};

/**
 * Queued event information
 * Contains all data needed to process an event
 */
struct QueuedEvent {
    BridgeEvent eventType;          // Type of event (from BridgeSystemDefs.h enum)
    EventData* eventData;           // Additional data associated with the event
    EventPriority priority;         // Priority level (NORMAL or EMERGENCY)
    unsigned long timestamp;        // When the event was queued (milliseconds)
};

/**
 * EventBus - Central messaging system for event-driven architecture
 * 
 * Handles event subscription, publishing, and priority-based processing
 * Events represent facts about what has already happened (e.g., BOAT_DETECTED)
 * 
 * Thread-safe implementation with mutex protection
 */
class EventBus {
public:
    EventBus();
    ~EventBus();
    
    // Core functionality methods - all thread-safe
    
    // Registers a function to be called when the specified event type occurs
    // priority parameter determines if this is a normal or emergency handler
    void subscribe(BridgeEvent eventType, std::function<void(EventData*)> callback, EventPriority priority = EventPriority::NORMAL);
    
    // Adds an event to the processing queue
    // eventData can contain additional information about the event
    // priority determines if this event is processed before normal events
    void publish(BridgeEvent eventType, EventData* eventData = nullptr, EventPriority priority = EventPriority::NORMAL);
    
    // Removes a previously registered callback for an event
    void unsubscribe(BridgeEvent eventType, std::function<void(EventData*)> callback);
    
    // Event processing - thread-safe
    
    // Processes all pending events in priority order
    // EMERGENCY events are processed before any NORMAL events
    // Call this method regularly from the "main" loop
    void processEvents();
    
    // Utility methods - thread-safe
    
    // Removes all events and optionally all subscriptions
    void clear();
    
    // Returns true if the event type has any subscribers
    bool hasSubscriptions(BridgeEvent eventType) const;

private:
    // Maps event types to their list of subscribers
    std::map<BridgeEvent, std::vector<EventSubscription>> subscribers;
    
    // Queue of events waiting to be processed
    std::vector<QueuedEvent> eventQueue;
    
    // Mutexes for thread synchronization
    mutable std::mutex subscribers_mutex;  // Protects subscribers map
    std::mutex eventQueue_mutex;           // Protects event queue
};

// Global instance declaration
extern EventBus eventBus;