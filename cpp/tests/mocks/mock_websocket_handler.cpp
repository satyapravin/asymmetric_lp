#include "mock_websocket_handler.hpp"
#include <iostream>
#include <chrono>
#include <thread>

MockWebSocketHandler::MockWebSocketHandler(const std::string& test_data_dir) 
    : test_data_dir_(test_data_dir), running_(false) {
}

MockWebSocketHandler::~MockWebSocketHandler() {
    shutdown();
}

bool MockWebSocketHandler::connect(const std::string& url) {
    if (connection_failure_enabled_) {
        return false;
    }
    
    std::this_thread::sleep_for(connection_delay_);
    connected_.store(true);
    
    if (connect_callback_) {
        connect_callback_(true);
    }
    
    return true;
}

void MockWebSocketHandler::disconnect() {
    connected_.store(false);
    
    if (connect_callback_) {
        connect_callback_(false);
    }
}

bool MockWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[MOCK_WS] Sending message: " << message << std::endl;
    return true;
}

bool MockWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[MOCK_WS] Sending binary data: " << data.size() << " bytes" << std::endl;
    return true;
}

void MockWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void MockWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void MockWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void MockWebSocketHandler::set_ping_interval(int seconds) {
    // Mock implementation
}

void MockWebSocketHandler::set_timeout(int seconds) {
    // Mock implementation
}

void MockWebSocketHandler::set_reconnect_attempts(int attempts) {
    // Mock implementation
}

void MockWebSocketHandler::set_reconnect_delay(int seconds) {
    // Mock implementation
}

bool MockWebSocketHandler::initialize() {
    return true;
}

void MockWebSocketHandler::shutdown() {
    running_.store(false);
    if (message_thread_.joinable()) {
        message_thread_.join();
    }
}

WebSocketState MockWebSocketHandler::get_state() const {
    return connected_.load() ? WebSocketState::CONNECTED 
                             : WebSocketState::DISCONNECTED;
}

void MockWebSocketHandler::simulate_message(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.is_binary = false;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_message.channel = "";
        
        message_callback_(ws_message);
    }
}

void MockWebSocketHandler::simulate_message_from_file(const std::string& filename) {
    std::string message = load_message_from_file(filename);
    if (!message.empty()) {
        simulate_message(message);
    }
}

void MockWebSocketHandler::simulate_connection_event(bool connected) {
    connected_.store(connected);
    if (connect_callback_) {
        connect_callback_(connected);
    }
}

void MockWebSocketHandler::simulate_error(const std::string& error) {
    if (error_callback_) {
        error_callback_(-1, error);
    }
}

void MockWebSocketHandler::message_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string MockWebSocketHandler::load_message_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "[MOCK_WS] Could not open file: " << file_path << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}