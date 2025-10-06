cmake_minimum_required(VERSION 3.14)

# Project definition
project(DetectionSystemTest)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to GoogleTest
# Path to GoogleTest (try teammate’s first, then yours)
if (EXISTS "/media/ibromr/Yedek/SystemCoding/Macquarie/GoogleTest/googletest")
    set(GOOGLETEST_DIR "/media/ibromr/Yedek/SystemCoding/Macquarie/GoogleTest/googletest")
elseif (EXISTS "/Users/cnn/Desktop/gtest/googletest")
    set(GOOGLETEST_DIR "/Users/cnn/Desktop/gtest/googletest")
endif()

# Only add if we found something
if (GOOGLETEST_DIR)
    add_subdirectory(${GOOGLETEST_DIR} googletest_build)
else()
    message(FATAL_ERROR "Could not find GoogleTest directory")
endif()

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
