cmake_minimum_required(VERSION 3.14)

# Project definition
project(DetectionSystemTest)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to GoogleTest
set(GOOGLETEST_DIR "/media/ibromr/Yedek/SystemCoding/Macquarie/GoogleTest/googletest")
add_subdirectory(${GOOGLETEST_DIR} googletest_build)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Test executable (only one entry)
add_executable(test_detection_system 
    test/test_detection_system.cpp
    src/DetectionSystem.cpp
    src/EventBus.cpp  # If you have EventBus implementation
)

# Link with GoogleTest
target_link_libraries(test_detection_system PRIVATE gtest_main)

# Run tests
include(GoogleTest)
gtest_discover_tests(test_detection_system)

# Define UNIT_TEST for compilation
add_definitions(-DUNIT_TEST)
