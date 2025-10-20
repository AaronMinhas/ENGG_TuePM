// This is mock test area
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#ifdef UNIT_TEST
extern unsigned long mock_millis;
inline unsigned long millis() { return mock_millis; }
#endif

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#include "EventBus.h"
#include "Logger.h"

// Global instance
EventBus eventBus;

// EventData implementation
EventData::~EventData() {
    // Virtual destructor for proper cleanup of derived classes
    // Cleanup code here if needed
}

const char* EventData::getEventType() const {
    // Default implementation returns a generic type
    // Derived classes should override this with specific event names
    // Implementation here
    
    return "EventData"; // Placeholder return value
}

BridgeEvent EventData::getEventEnum() const {
    // Default implementation returns a safe default value
    // Derived classes should override this with their specific event type
    // Implementation here
    
    return BridgeEvent::FAULT_DETECTED; // Placeholder return value
}

// SimpleEventData
const char* SimpleEventData::getEventType() const {
    return bridgeEventToString(eventType_);
}

// EventBus implementation
EventBus::EventBus() {
    // Initialize event bus
    eventQueue.clear();
    subscribers.clear();
}

EventBus::~EventBus() {
    // Acquarie locks to safely clean up shared resources
    std::lock_guard<std::recursive_mutex> queueLock(eventQueue_mutex);
    std::lock_guard<std::recursive_mutex> subscribersLock(subscribers_mutex);

    // Delete all pending events in the queue
    for (auto& queuedEvent : eventQueue) {
        // Clean up any heap-allocated EventData
        delete queuedEvent.eventData;
        queuedEvent.eventData = nullptr;
    }
    eventQueue.clear();

    // Clear all subscribers for each event type
    subscribers.clear();
}

void EventBus::subscribe(BridgeEvent eventType, std::function<void(EventData*)> callback, EventPriority priority) {
    // Lock the subscribers map for thread safety
    std::lock_guard<std::recursive_mutex> lock(subscribers_mutex);

    //Create a new subscribtion object
    EventSubscription subscribtion;
    subscribtion.callback = callback;
    subscribtion.priority = priority;

    // Add the subscription to the list for the given event type
    subscribers[eventType].push_back(subscribtion);
}

void EventBus::publish(BridgeEvent eventType, EventData* eventData, EventPriority priority) {
    // Lock the event queue for thread safety (recursive mutex allows nested calls)
    std::lock_guard<std::recursive_mutex> lock(eventQueue_mutex);

    // Create a new QueuedEvent with the provided data
    QueuedEvent newEvent;
    newEvent.eventType = eventType;
    newEvent.eventData = eventData;
    newEvent.priority = priority;
    newEvent.timestamp = millis(); // Save Event starting time

    // Add it to the event queue based on priority
    if (priority == EventPriority::EMERGENCY) {
        // Insert after the last EMERGENCY event, but before NORMAL events
        auto it = eventQueue.begin();
        while (it != eventQueue.end() && it->priority == EventPriority::EMERGENCY) {
            ++it;
        }
        eventQueue.insert(it, newEvent);
    } else {
        // NORMAL events go to the back of the queue
        eventQueue.push_back(newEvent);
    }
}

void EventBus::unsubscribe(BridgeEvent eventType, std::function<void(EventData*)> callback) {
     // Lock the subscribers map for thread safety
    std::lock_guard<std::recursive_mutex> lock(subscribers_mutex);

    // Since std::function comparison is not supported on ESP32,
    // We now remove all subscribers for this event type
    // This works for our use case where we subscribe once and don't unsubscribe.
    auto it = subscribers.find(eventType);
    if (it != subscribers.end()) {
        subscribers.erase(it);
        LOG_WARN(Logger::TAG_EVT, "Removed all subscribers for event type");
    }
}

void EventBus::processEvents() {
    // Lock both event queue and subscribers for thread safety
    std::lock_guard<std::recursive_mutex> queueLock(eventQueue_mutex);
    std::lock_guard<std::recursive_mutex> subscribersLock(subscribers_mutex);

    // Process events in the queue
    while (!eventQueue.empty()) {
        // 1. Get events from the queue (with appropriate locking)
        QueuedEvent event = eventQueue.front();
        eventQueue.erase(eventQueue.begin());

        // 2. For each event, find subscribers and call their callbacks
        auto subIt = subscribers.find(event.eventType);
        if (subIt != subscribers.end()) {
            // Only log events that have actual impact (non-debug events)
            if ((int)event.eventType < 16) { // Log important events only
                LOG_DEBUG(Logger::TAG_EVT, "EVENT: %s → %d subscribers",
                          bridgeEventToString(event.eventType), subIt->second.size());
            }
            
            for (const auto& subscription : subIt->second) {
                if (subscription.callback) {
                    subscription.callback(event.eventData);
                }
            }
        }
        
        // Clean up event data if provided
        delete event.eventData;
        event.eventData = nullptr;
    }
}

void EventBus::clear() {
    // Lock both event queue and subscribers for thread safety
    std::lock_guard<std::recursive_mutex> queueLock(eventQueue_mutex);
    std::lock_guard<std::recursive_mutex> subscribersLock(subscribers_mutex);

    // Delete all pending events in the queue
    for (auto& event : eventQueue) {
        delete event.eventData;
        event.eventData = nullptr;
    }
    eventQueue.clear();

    // Clear all subscribers
    subscribers.clear();
}

bool EventBus::hasSubscriptions(BridgeEvent eventType) const {
    std::lock_guard<std::recursive_mutex> lock(subscribers_mutex);

    auto it = subscribers.find(eventType);
    return (it != subscribers.end() && !it->second.empty());
}
