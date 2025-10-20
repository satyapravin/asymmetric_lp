#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"

int main() {
    std::cout << "=== WebSocket Transport Abstraction Test ===" << std::endl;
    
    // Create Binance WebSocket handler
    auto handler = std::make_unique<binance::BinancePublicWebSocketHandler>();
    
    // Configure for testing with mock transport
    handler->configure_mock_transport("../../tests/data", 50, 100);
    
    // Set up callbacks
    handler->set_message_callback([](const WebSocketMessage& message) {
        std::cout << "[CALLBACK] Received message: " << message.data << std::endl;
    });
    
    handler->set_error_callback([](const std::string& error) {
        std::cerr << "[CALLBACK] Error: " << error << std::endl;
    });
    
    handler->set_connect_callback([](bool connected) {
        std::cout << "[CALLBACK] Connection status: " << (connected ? "connected" : "disconnected") << std::endl;
    });
    
    // Initialize and connect
    if (!handler->initialize()) {
        std::cerr << "Failed to initialize handler" << std::endl;
        return 1;
    }
    
    std::cout << "Connecting to Binance WebSocket..." << std::endl;
    if (!handler->connect("wss://stream.binance.com:9443/ws/btcusdt@trade")) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Wait a bit for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    if (handler->is_connected()) {
        std::cout << "Successfully connected!" << std::endl;
        
        // Subscribe to orderbook
        if (handler->subscribe_to_orderbook("BTCUSDT")) {
            std::cout << "Subscribed to BTCUSDT orderbook" << std::endl;
        }
        
        // Subscribe to trades
        if (handler->subscribe_to_trades("BTCUSDT")) {
            std::cout << "Subscribed to BTCUSDT trades" << std::endl;
        }
        
        // Simulate some messages (this would be done by the mock transport)
        std::cout << "Simulating market data messages..." << std::endl;
        
        // Wait for messages
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Disconnect
        handler->disconnect();
        std::cout << "Disconnected" << std::endl;
    } else {
        std::cerr << "Failed to establish connection" << std::endl;
        return 1;
    }
    
    handler->shutdown();
    
    std::cout << "=== Test completed successfully ===" << std::endl;
    return 0;
}
