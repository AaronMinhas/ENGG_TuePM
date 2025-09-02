#ifdef UNIT_TEST
unsigned long mock_millis = 0;
#endif

#include <gtest/gtest.h>
#include "DetectionSystem.h"


// Mock EventBus to capture published events
class MockEventBus : public EventBus {
public:
    BridgeEvent lastEvent = BridgeEvent::FAULT_DETECTED; // Use an existing enum value
    void publish(BridgeEvent eventType, EventData* eventData = nullptr, EventPriority priority = EventPriority::NORMAL) /* no override */ {
        lastEvent = eventType;
    }
};

// Test: Check if the DetectionSystem initializes correctly
TEST(DetectionSystemTest, InitializationTest) {
    // Arrange: Create a mock EventBus for testing
    MockEventBus mockEventBus;
    
    // Act: Create DetectionSystem with the mock EventBus
    DetectionSystem system(mockEventBus);
    
    // Initialize the system
    system.begin();
     // Simulate time passing for debounce
    //for (int i = 0; i < 2; ++i) {
        mock_millis = 1001; // Advance time by debounceDelay
        system.update();
    //}


    // Assert: Check if the system is initialized correctly (assuming isInitialized() exists)
    EXPECT_TRUE(system.isInitialized());
    EXPECT_EQ(mockEventBus.lastEvent, BridgeEvent::BOAT_DETECTED);
}
