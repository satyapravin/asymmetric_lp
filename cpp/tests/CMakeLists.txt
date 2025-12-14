# Copy test config and data files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_config.ini 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy config files from config subdirectory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Mock WebSocket transport for testing
add_library(test_mocks STATIC
    mocks/mock_websocket_transport.hpp
    mocks/mock_websocket_transport.cpp
)

target_include_directories(test_mocks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)


# Create main test runner executable
add_executable(run_tests
    test_runner.cpp
)

# Link libraries
target_link_libraries(run_tests
    test_mocks
    exchanges
    utils
    proto_msgs
    trader_lib
    market_making_strategy
    trading_engine_lib
    market_server_lib
    position_server_lib
)

# Include directories
target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add to test suite
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Custom target to run all tests as part of build
add_custom_target(run_all_tests
    COMMAND run_tests
    DEPENDS run_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all unit and integration tests"
)

# Make tests run after build
add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS run_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all tests with CTest"
)