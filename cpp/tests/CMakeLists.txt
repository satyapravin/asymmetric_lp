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
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)