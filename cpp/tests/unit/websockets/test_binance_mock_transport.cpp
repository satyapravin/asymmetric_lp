#include "doctest.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "../../mocks/mock_websocket_transport.hpp"

using namespace test_utils;

TEST_CASE("Mock WebSocket Transport Basic Functionality") {
    std::cout << "[TEST] Testing Mock WebSocket Transport" << std::endl;
    
    // Create mock transport
    auto transport = TestWebSocketTransportFactory::create_mock_with_data("data/binance/websocket");
    REQUIRE(transport != nullptr);
    
    // Test initial state
    CHECK_FALSE(transport->is_connected());
    CHECK(transport->get_state() == websocket_transport::WebSocketState::DISCONNECTED);
    
    // Test connection
    bool connected = transport->connect("wss://test.binance.com/ws");
    CHECK(connected);
    CHECK(transport->is_connected());
    CHECK(transport->get_state() == websocket_transport::WebSocketState::CONNECTED);
    
    // Test disconnection
    transport->disconnect();
    CHECK_FALSE(transport->is_connected());
    CHECK(transport->get_state() == websocket_transport::WebSocketState::DISCONNECTED);
}

TEST_CASE("Mock WebSocket Transport Message Simulation") {
    std::cout << "[TEST] Testing Mock WebSocket Transport Message Simulation" << std::endl;
    
    auto transport = TestWebSocketTransportFactory::create_mock_with_data("data/binance/websocket");
    REQUIRE(transport != nullptr);
    
    // Set up callbacks
    int message_count = 0;
    int error_count = 0;
    bool connection_status = false;
    
    transport->set_message_callback([&message_count](const websocket_transport::WebSocketMessage& message) {
        std::cout << "[TEST] Received message: " << message.data.substr(0, 100) << "..." << std::endl;
        message_count++;
    });
    
    transport->set_error_callback([&error_count](int error_code, const std::string& error_message) {
        std::cout << "[TEST] Error: " << error_code << " - " << error_message << std::endl;
        error_count++;
    });
    
    transport->set_connect_callback([&connection_status](bool connected) {
        std::cout << "[TEST] Connection status: " << (connected ? "CONNECTED" : "DISCONNECTED") << std::endl;
        connection_status = connected;
    });
    
    // Connect
    bool connected = transport->connect("wss://test.binance.com/ws");
    CHECK(connected);
    CHECK(connection_status);
    
    // Start event loop
    transport->start_event_loop();
    
    // Simulate different types of messages
    auto* mock_transport = TestWebSocketTransportFactory::cast_to_mock(transport.get());
    REQUIRE(mock_transport != nullptr);
    
    mock_transport->simulate_orderbook_message("BTCUSDT");
    mock_transport->simulate_trade_message("BTCUSDT");
    mock_transport->simulate_ticker_message("BTCUSDT");
    
    // Wait for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop event loop
    transport->stop_event_loop();
    
    // Check that messages were received
    CHECK(message_count > 0);
}

TEST_CASE("Mock WebSocket Transport JSON File Loading") {
    std::cout << "[TEST] Testing Mock WebSocket Transport JSON File Loading" << std::endl;
    
    auto transport = TestWebSocketTransportFactory::create_mock_with_data("data/binance/websocket");
    REQUIRE(transport != nullptr);
    
    auto* mock_transport = TestWebSocketTransportFactory::cast_to_mock(transport.get());
    REQUIRE(mock_transport != nullptr);
    
    // Set up message callback
    int message_count = 0;
    transport->set_message_callback([&message_count](const websocket_transport::WebSocketMessage& message) {
        std::cout << "[TEST] Loaded JSON message: " << message.data.substr(0, 100) << "..." << std::endl;
        message_count++;
    });
    
    // Connect and start event loop
    transport->connect("wss://test.binance.com/ws");
    transport->start_event_loop();
    
    // Test loading specific files
    mock_transport->load_and_replay_json_file("data/binance/websocket/orderbook_snapshot_message.json");
    mock_transport->load_and_replay_json_file("data/binance/websocket/trade_message.json");
    
    // Wait for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop event loop
    transport->stop_event_loop();
    
    // Check that JSON files were loaded
    CHECK(message_count > 0);
}
