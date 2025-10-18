# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(simdjson REQUIRED)

# Add test executable
add_executable(run_tests
  test_runner.cpp
  # Test Configuration Manager
  config/test_config_manager.cpp
  # Exchange WebSocket Handler Tests
  unit/exchanges/binance/public_websocket/test_binance_public_websocket_handler.cpp
  unit/exchanges/binance/private_websocket/test_binance_private_websocket_handler.cpp
  unit/exchanges/binance/http/test_binance_data_fetcher.cpp
  unit/exchanges/binance/http/test_binance_oms.cpp
  unit/exchanges/binance/test_authentication.cpp
  # Configuration System Tests
  unit/config/test_configuration_system.cpp
  # Utility Component Tests
  unit/utils/test_utility_components.cpp
  # Protocol Buffer Tests
  unit/protobuf/test_protocol_buffers.cpp
  # Process-Specific Tests
  unit/processes/test_process_specific.cpp
  # Integration Tests
  integration/workflows/test_end_to_end_workflows.cpp
  # Performance Tests
  performance/test_performance.cpp
  # Security Tests
  security/test_security.cpp
)

# Link with all required libraries
target_link_libraries(run_tests PRIVATE 
  utils 
  position_server_lib
  trader_lib
  exchanges
  quote_server_lib
  simdjson::simdjson
  ${ZeroMQ_LIBRARIES}
  ${Protobuf_LIBRARIES}
  ssl
  crypto
  websockets
  curl
  OpenSSL::SSL
  OpenSSL::Crypto
)

# Include directories
target_include_directories(run_tests PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../utils
  ${CMAKE_CURRENT_SOURCE_DIR}/../position_server
  ${CMAKE_CURRENT_SOURCE_DIR}/../trader
  ${CMAKE_CURRENT_SOURCE_DIR}/../trader/models
  ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
  ${CMAKE_CURRENT_SOURCE_DIR}/config
  ${ZeroMQ_INCLUDE_DIRS}
  ${Protobuf_INCLUDE_DIRS}
)

# Add test to CTest
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
  TIMEOUT 30
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
