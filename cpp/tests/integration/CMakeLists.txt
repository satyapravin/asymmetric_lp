# Integration Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(doctest REQUIRED)
find_package(ZMQ REQUIRED)
find_package(jsoncpp REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/integration
    ${CMAKE_SOURCE_DIR}/tests/unit/framework/interfaces
    ${CMAKE_SOURCE_DIR}/exchanges
    ${CMAKE_SOURCE_DIR}/trader
    ${CMAKE_SOURCE_DIR}/strategies/base_strategy
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_SOURCE_DIR}/proto
)

# WebSocket Transport Test
add_executable(websocket_transport_test
    websocket_transport_test.cpp
)

target_link_libraries(websocket_transport_test
    exchanges
    proto_msgs
)

target_include_directories(websocket_transport_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

# Source files
set(INTEGRATION_TEST_SOURCES
    integration_test_main.cpp
    test_strategy.cpp
    test_container.cpp
)

# Create integration test executable
add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})

# Link libraries
target_link_libraries(integration_tests
    doctest::doctest
    ${Protobuf_LIBRARIES}
    ${ZMQ_LIBRARIES}
    jsoncpp_lib
    pthread
)

# Compiler flags
target_compile_features(integration_tests PRIVATE cxx_std_17)
target_compile_options(integration_tests PRIVATE -Wall -Wextra -O2)

# Add to test suite
enable_testing()
add_test(NAME integration_tests COMMAND integration_tests)

# Custom target to run integration tests
add_custom_target(run_integration_tests
    COMMAND integration_tests
    DEPENDS integration_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running integration tests"
)
