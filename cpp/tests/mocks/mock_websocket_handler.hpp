#pragma once
#include "../../utils/websocket/i_websocket_handler.hpp"
#include <fstream>
#include <filesystem>
#include <queue>
#include <thread>
#include <atomic>

class MockWebSocketHandler : public IWebSocketHandler {
public:
    MockWebSocketHandler(const std::string& test_data_dir);
    ~MockWebSocketHandler();
    
    // IWebSocketHandler interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override { return connected_; }
    
    bool send_message(const std::string& message) override;
    void set_message_callback(WebSocketMessageCallback callback) override;
    void set_connection_callback(WebSocketConnectionCallback callback) override;
    void set_error_callback(WebSocketErrorCallback callback) override;
    
    // Test configuration
    void set_message_delay(std::chrono::milliseconds delay) { message_delay_ = delay; }
    void set_connection_delay(std::chrono::milliseconds delay) { connection_delay_ = delay; }
    void enable_connection_failure(bool enable) { connection_failure_enabled_ = enable; }
    
    // Simulate incoming messages
    void simulate_message(const std::string& message);
    void simulate_message_from_file(const std::string& filename);
    void simulate_connection_event(bool connected);
    void simulate_error(const std::string& error);
    
private:
    std::string test_data_dir_;
    std::atomic<bool> connected_{false};
    std::chrono::milliseconds message_delay_{0};
    std::chrono::milliseconds connection_delay_{0};
    bool connection_failure_enabled_{false};
    
    WebSocketMessageCallback message_callback_;
    WebSocketConnectionCallback connection_callback_;
    WebSocketErrorCallback error_callback_;
    
    std::queue<std::string> message_queue_;
    std::thread message_thread_;
    std::atomic<bool> running_{false};
    
    void message_loop();
    std::string load_message_from_file(const std::string& file_path);
};
