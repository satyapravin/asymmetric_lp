#include "mock_websocket_handler.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

MockWebSocketHandler::MockWebSocketHandler(const std::string& test_data_dir) 
    : test_data_dir_(test_data_dir) {
}

MockWebSocketHandler::~MockWebSocketHandler() {
    disconnect();
}

bool MockWebSocketHandler::connect(const std::string& url) {
    if (connection_failure_enabled_) {
        if (error_callback_) {
            error_callback_("Connection failure simulation");
        }
        return false;
    }
    
    // Simulate connection delay
    if (connection_delay_.count() > 0) {
        std::this_thread::sleep_for(connection_delay_);
    }
    
    connected_ = true;
    running_ = true;
    
    // Start message processing thread
    message_thread_ = std::thread(&MockWebSocketHandler::message_loop, this);
    
    // Notify connection callback
    if (connection_callback_) {
        connection_callback_(true);
    }
    
    return true;
}

void MockWebSocketHandler::disconnect() {
    if (!connected_) return;
    
    connected_ = false;
    running_ = false;
    
    if (message_thread_.joinable()) {
        message_thread_.join();
    }
    
    // Clear message queue
    while (!message_queue_.empty()) {
        message_queue_.pop();
    }
    
    // Notify connection callback
    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool MockWebSocketHandler::send_message(const std::string& message) {
    if (!connected_) return false;
    
    // For testing, we just log the sent message
    std::cout << "[MOCK_WS] Sent: " << message << std::endl;
    return true;
}

void MockWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void MockWebSocketHandler::set_connection_callback(WebSocketConnectionCallback callback) {
    connection_callback_ = callback;
}

void MockWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void MockWebSocketHandler::simulate_message(const std::string& message) {
    if (!connected_) return;
    
    message_queue_.push(message);
}

void MockWebSocketHandler::simulate_message_from_file(const std::string& filename) {
    if (!connected_) return;
    
    std::string file_path = test_data_dir_ + "/" + filename;
    std::string message = load_message_from_file(file_path);
    
    if (!message.empty()) {
        message_queue_.push(message);
    }
}

void MockWebSocketHandler::simulate_connection_event(bool connected) {
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

void MockWebSocketHandler::simulate_error(const std::string& error) {
    if (error_callback_) {
        error_callback_(error);
    }
}

void MockWebSocketHandler::message_loop() {
    while (running_.load()) {
        if (!message_queue_.empty()) {
            std::string message = message_queue_.front();
            message_queue_.pop();
            
            // Simulate message delay
            if (message_delay_.count() > 0) {
                std::this_thread::sleep_for(message_delay_);
            }
            
            // Call message callback
            if (message_callback_) {
                message_callback_(message);
            }
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string MockWebSocketHandler::load_message_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "[MOCK_WS] Failed to open file: " << file_path << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    return content;
}
