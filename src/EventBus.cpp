#include "EventBus.h"

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

// EventBus implementation
EventBus::EventBus() {
    // Initialize event bus
    eventQueue.clear();
    subscribers.clear();
}

EventBus::~EventBus() {
    // Acquarie locks to safely clean up shared resources
    std::lock_guard<std::mutex> queueLock(eventQueue_mutex);
    std::lock_guard<std::mutex> subscribersLock(subscribers_mutex);

    // Delete all pending events in the queue
    for (auto& queuedEvent : eventQueue) {
        // If eventData was dynamically allocated, delete it 
        delete queuedEvent.eventData;
        queuedEvent.eventData = nullptr;
    }
    eventQueue.clear();

    // Clear all subscribers for each event type
    subscribers.clear();
}

void EventBus::subscribe(BridgeEvent eventType, std::function<void(EventData*)> callback, EventPriority priority) {
    // Lock the subscribers map for thread safety
    std::lock_guard<std::mutex> lock(subscribers_mutex);

    //Create a new subscribtion object
    EventSubscription subscribtion;
    subscribtion.callback = callback;
    subscribtion.priority = priority;

    // Add the subscription to the list for the given event type
    subscribers[eventType].push_back(subscribtion);
}

void EventBus::publish(BridgeEvent eventType, EventData* eventData, EventPriority priority) {
    // Lock the event queue for thread safety
    std::lock_guard<std::mutex> lock(eventQueue_mutex);

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
    std::lock_guard<std::mutex> lock(subscribers_mutex);

    // Find the event type in the subscribers map
    auto it = subscribers.find(eventType);
    if (it != subscribers.end()) {
        // Remove the callback from the list
        auto& vec = it->second;
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [&](const EventSubscription& sub) {
                    // Compare target type and address for std::function
                    return sub.callback.target_type() == callback.target_type();
                }),
            vec.end()
        );
        // If no more subscribers for this event, remove the key from the map
        if (vec.empty()) {
            subscribers.erase(it);
        }
    }
}

void EventBus::processEvents() {
    // This method should:
    // 1. Get events from the queue (with appropriate locking)
    // 2. For each event, find subscribers and call their callbacks
    // 3. Process EMERGENCY events before NORMAL events
    
    // IMPORTANT: Use mutex to protect access to event queue and subscribers map
    // std::lock_guard<std::mutex> queueLock(eventQueue_mutex);
    // std::lock_guard<std::mutex> subscribersLock(subscribers_mutex);
    
    // Event processing logic here
}

void EventBus::clear() {
    // This method should:
    // 1. Clear the event queue
    // 2. Optionally clear all subscriptions
    
    // IMPORTANT: Use mutex to protect access to event queue and subscribers map
    // std::lock_guard<std::mutex> queueLock(eventQueue_mutex);
    // std::lock_guard<std::mutex> subscribersLock(subscribers_mutex);
    
    // Clear logic here
}

bool EventBus::hasSubscriptions(BridgeEvent eventType) const {
    // This method should:
    // 1. Check if the event type has any subscribers
    // 2. Return true if it does, false otherwise
    
    // IMPORTANT: Use mutex to protect access to subscribers map
    // std::lock_guard<std::mutex> lock(subscribers_mutex);
    
    // Check logic here
    
    return false; // Placeholder return value
}