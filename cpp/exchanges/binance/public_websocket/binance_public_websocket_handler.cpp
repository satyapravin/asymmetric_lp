#include "binance_public_websocket_handler.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace binance {

BinancePublicWebSocketHandler::BinancePublicWebSocketHandler() {
    std::cout << "[BINANCE] Initializing Binance Public WebSocket Handler" << std::endl;
}

BinancePublicWebSocketHandler::~BinancePublicWebSocketHandler() {
    std::cout << "[BINANCE] Destroying Binance Public WebSocket Handler" << std::endl;
    disconnect();
}

bool BinancePublicWebSocketHandler::connect(const std::string& url) {
    std::cout << "[BINANCE] Connecting to public WebSocket: " << url << std::endl;
    
    websocket_url_ = url;
    state_.store(WebSocketState::CONNECTING);
    
    // Start connection thread
    connection_thread_running_.store(true);
    connection_thread_ = std::thread(&BinancePublicWebSocketHandler::connection_loop, this);
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (connected_.load()) {
        state_.store(WebSocketState::CONNECTED);
        if (connect_callback_) {
            connect_callback_(true);
        }
        std::cout << "[BINANCE] Connected successfully" << std::endl;
        return true;
    } else {
        state_.store(WebSocketState::ERROR);
        if (connect_callback_) {
            connect_callback_(false);
        }
        std::cerr << "[BINANCE] Failed to connect" << std::endl;
        return false;
    }
}

void BinancePublicWebSocketHandler::disconnect() {
    std::cout << "[BINANCE] Disconnecting from public WebSocket" << std::endl;
    
    connected_.store(false);
    state_.store(WebSocketState::DISCONNECTING);
    should_stop_.store(true);
    
    // Stop connection thread
    connection_thread_running_.store(false);
    if (connection_thread_.joinable()) {
        connection_thread_.join();
    }
    
    state_.store(WebSocketState::DISCONNECTED);
    std::cout << "[BINANCE] Disconnected" << std::endl;
}

bool BinancePublicWebSocketHandler::is_connected() const {
    return connected_.load();
}

WebSocketState BinancePublicWebSocketHandler::get_state() const {
    return state_.load();
}

bool BinancePublicWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot send message: not connected" << std::endl;
        return false;
    }
    
    std::cout << "[BINANCE] Sending message: " << message << std::endl;
    // In a real implementation, you would send the message via WebSocket
    return true;
}

bool BinancePublicWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot send binary data: not connected" << std::endl;
        return false;
    }
    
    std::cout << "[BINANCE] Sending binary data: " << data.size() << " bytes" << std::endl;
    // In a real implementation, you would send the binary data via WebSocket
    return true;
}

void BinancePublicWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
    std::cout << "[BINANCE] Message callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
    std::cout << "[BINANCE] Error callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
    std::cout << "[BINANCE] Connect callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_ping_interval(int seconds) {
    ping_interval_.store(seconds);
    std::cout << "[BINANCE] Ping interval set to: " << seconds << " seconds" << std::endl;
}

void BinancePublicWebSocketHandler::set_timeout(int seconds) {
    timeout_.store(seconds);
    std::cout << "[BINANCE] Timeout set to: " << seconds << " seconds" << std::endl;
}

void BinancePublicWebSocketHandler::set_reconnect_attempts(int attempts) {
    reconnect_attempts_.store(attempts);
    std::cout << "[BINANCE] Reconnect attempts set to: " << attempts << std::endl;
}

void BinancePublicWebSocketHandler::set_reconnect_delay(int seconds) {
    reconnect_delay_.store(seconds);
    std::cout << "[BINANCE] Reconnect delay set to: " << seconds << " seconds" << std::endl;
}

bool BinancePublicWebSocketHandler::initialize() {
    std::cout << "[BINANCE] Initializing public WebSocket handler" << std::endl;
    return true;
}

void BinancePublicWebSocketHandler::shutdown() {
    std::cout << "[BINANCE] Shutting down public WebSocket handler" << std::endl;
    disconnect();
}

bool BinancePublicWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot subscribe: not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    subscribed_channels_.push_back(channel);
    
    std::cout << "[BINANCE] Subscribed to channel: " << channel << std::endl;
    return true;
}

bool BinancePublicWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot unsubscribe: not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[BINANCE] Unsubscribed from channel: " << channel << std::endl;
        return true;
    }
    
    std::cerr << "[BINANCE] Channel not found: " << channel << std::endl;
    return false;
}

std::vector<std::string> BinancePublicWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void BinancePublicWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    // Public streams don't require authentication
    std::cout << "[BINANCE] Public WebSocket doesn't require authentication" << std::endl;
}

bool BinancePublicWebSocketHandler::subscribe_to_orderbook(const std::string& symbol) {
    std::string channel = symbol + "@depth20@100ms";
    return subscribe_to_channel(channel);
}

bool BinancePublicWebSocketHandler::subscribe_to_trades(const std::string& symbol) {
    std::string channel = symbol + "@trade";
    return subscribe_to_channel(channel);
}

bool BinancePublicWebSocketHandler::subscribe_to_ticker(const std::string& symbol) {
    std::string channel = symbol + "@ticker";
    return subscribe_to_channel(channel);
}

void BinancePublicWebSocketHandler::handle_message(const std::string& message) {
    handle_websocket_message(message);
}

void BinancePublicWebSocketHandler::connection_loop() {
    std::cout << "[BINANCE] Starting connection loop" << std::endl;
    
    // Simulate connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    connected_.store(true);
    
    // Keep connection alive
    while (connection_thread_running_.load() && !should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(ping_interval_.load()));
        
        if (connection_thread_running_.load()) {
            // Send ping
            std::cout << "[BINANCE] Sending ping" << std::endl;
        }
    }
    
    connected_.store(false);
    std::cout << "[BINANCE] Connection loop stopped" << std::endl;
}

void BinancePublicWebSocketHandler::handle_websocket_message(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.is_binary = false;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_message.channel = "public";
        
        message_callback_(ws_message);
    }
    
    std::cout << "[BINANCE] Received message: " << message << std::endl;
}

} // namespace binance