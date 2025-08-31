#include <gtest/gtest.h>
#include "DetectionSystem.h"

// Mock class for EventBus (using real EventBus in your actual code)
class MockEventBus : public EventBus {
public:
    MockEventBus() {}
    ~MockEventBus() {}

    // You can override necessary methods if EventBus has specific logic for your tests
};

// Test: Check if the DetectionSystem initializes correctly
TEST(DetectionSystemTest, InitializationTest) {
    // Arrange: Create a mock EventBus for testing
    MockEventBus mockEventBus;
    
    // Act: Create DetectionSystem with the mock EventBus
    DetectionSystem system(mockEventBus);
    
    // Initialize the system
    system.begin();

    // Assert: Check if the system is initialized correctly (assuming isInitialized() exists)
    EXPECT_TRUE(system.isInitialized());  // This method should return true if initialized properly
}
