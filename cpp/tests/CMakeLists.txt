# Simplified Test CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(AsymmetricLPTests)

# Find required packages
find_package(simdjson REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Find CURL manually
set(CURL_LIBRARY "/usr/lib/x86_64-linux-gnu/libcurl.so.4")
set(CURL_INCLUDE_DIR "/home/pravin/anaconda3/include")
if(EXISTS ${CURL_LIBRARY} AND EXISTS ${CURL_INCLUDE_DIR})
    set(CURL_FOUND TRUE)
    message(STATUS "Found CURL: ${CURL_LIBRARY}")
    message(STATUS "Found CURL headers: ${CURL_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "CURL not found")
endif()

# Add doctest
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Create a minimal test executable that doesn't depend on the full system
add_executable(run_tests
  test_runner_standalone.cpp
  # Test Configuration Manager (standalone)
  config/test_config_manager_standalone.cpp
  # Protocol Buffer Tests (standalone)
  unit/protobuf/test_protocol_buffers_standalone.cpp
  # Binance Exchange Tests (framework-based)
  unit/exchanges/binance/test_binance_unit_tests.cpp
  # GRVT Exchange Tests (framework-based)
  unit/exchanges/grvt/test_grvt_unit_tests.cpp
  # Deribit Exchange Tests (framework-based)
  unit/exchanges/deribit/test_deribit_unit_tests.cpp
  # Framework Tests (using mock interfaces)
  unit/framework/test_framework_runner.cpp
  unit/framework/config/test_api_endpoint_manager.cpp
  unit/framework/utils/test_message_handler_manager.cpp
  unit/framework/utils/test_market_data_normalizer.cpp
  unit/framework/utils/test_exchange_monitor.cpp
  unit/framework/zmq/test_zmq_components.cpp
  unit/framework/websocket/test_websocket_handler.cpp
  unit/framework/integration/test_binance_testnet_integration.cpp
  # Strategy Tests
  unit/strategies/test_market_making_strategy.cpp
  # Exchange Implementation Source Files
  ../exchanges/binance/http/binance_data_fetcher.cpp
  ../exchanges/binance/public_websocket/binance_public_websocket_handler.cpp
  ../exchanges/binance/private_websocket/binance_private_websocket_handler.cpp
  ../exchanges/binance/private_websocket/binance_oms.cpp
  ../exchanges/binance/private_websocket/binance_pms.cpp
  ../exchanges/binance/public_websocket/binance_subscriber.cpp
  ../exchanges/grvt/http/grvt_data_fetcher.cpp
  ../exchanges/grvt/public_websocket/grvt_public_websocket_handler.cpp
  ../exchanges/grvt/private_websocket/grvt_private_websocket_handler.cpp
  ../exchanges/grvt/private_websocket/grvt_oms.cpp
  ../exchanges/grvt/private_websocket/grvt_pms.cpp
  ../exchanges/grvt/public_websocket/grvt_subscriber.cpp
  ../exchanges/deribit/http/deribit_data_fetcher.cpp
  ../exchanges/deribit/public_websocket/deribit_public_websocket_handler.cpp
  ../exchanges/deribit/private_websocket/deribit_private_websocket_handler.cpp
  ../exchanges/deribit/private_websocket/deribit_oms.cpp
  ../exchanges/deribit/private_websocket/deribit_pms.cpp
  ../exchanges/deribit/public_websocket/deribit_subscriber.cpp
  # Common exchange interfaces and utilities
  ../utils/http/curl_http_handler.cpp
  ../utils/websocket/libuv_websocket_handler.cpp
  ../exchanges/config/api_endpoint_config.cpp
  # Framework utilities for testing
  ../utils/handlers/message_handler_manager.cpp
  ../utils/handlers/message_handler.cpp
  ../utils/mds/market_data_normalizer.cpp
  ../utils/oms/exchange_monitor.cpp
  ../utils/zmq/zmq_publisher.cpp
  ../utils/zmq/zmq_subscriber.cpp
  # Strategy source files
  ../strategies/mm_strategy/market_making_strategy.cpp
  # Protocol Buffer source files
  ../proto/order.pb.cc
  ../proto/market_data.pb.cc
  ../proto/position.pb.cc
)

# Find libuv and protobuf
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)
find_package(Protobuf REQUIRED)

# Link with minimal required libraries
target_link_libraries(run_tests PRIVATE 
  simdjson::simdjson
  doctest::doctest
  OpenSSL::SSL
  OpenSSL::Crypto
  jsoncpp_lib
  ${CURL_LIBRARY}
  ${LIBUV_LIBRARIES}
  ${Protobuf_LIBRARIES}
  ${ZMQ_LIBRARIES}
)

# Include directories
target_include_directories(run_tests PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/config
  ${CURL_INCLUDE_DIR}
  ${LIBUV_INCLUDE_DIRS}
  ${ZMQ_INCLUDE_DIRS}
)

# Add test to CTest
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
  TIMEOUT 30
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
