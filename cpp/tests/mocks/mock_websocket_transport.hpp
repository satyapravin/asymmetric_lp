#pragma once
#include "../../exchanges/websocket/i_websocket_transport.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <filesystem>

namespace test_utils {

// Mock WebSocket Transport Implementation for testing
class MockWebSocketTransport : public websocket_transport::IWebSocketTransport {
public:
    MockWebSocketTransport();
    ~MockWebSocketTransport();
    
    // IWebSocketTransport interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    websocket_transport::WebSocketState get_state() const override;
    
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    bool send_ping() override;
    
    void set_message_callback(websocket_transport::WebSocketMessageCallback callback) override;
    void set_error_callback(websocket_transport::WebSocketErrorCallback callback) override;
    void set_connect_callback(websocket_transport::WebSocketConnectCallback callback) override;
    
    void set_ping_interval(int seconds) override;
    void set_timeout(int seconds) override;
    void set_reconnect_attempts(int attempts) override;
    void set_reconnect_delay(int seconds) override;
    
    bool initialize() override;
    void shutdown() override;
    
    void start_event_loop() override;
    void stop_event_loop() override;
    bool is_event_loop_running() const override;
    
    // Mock-specific methods for testing
    void set_test_data_directory(const std::string& directory);
    void set_simulation_delay_ms(int delay_ms);
    void set_connection_delay_ms(int delay_ms);
    
    // Simulate different types of messages
    void simulate_orderbook_message(const std::string& symbol);
    void simulate_trade_message(const std::string& symbol);
    void simulate_ticker_message(const std::string& symbol);
    void simulate_custom_message(const std::string& message);
    void simulate_connection_success();
    void simulate_connection_failure();
    void simulate_disconnection();
    void simulate_error(int error_code, const std::string& error_message);
    
    // Load and replay saved JSON files
    void load_and_replay_json_file(const std::string& filename);
    void load_and_replay_json_directory(const std::string& directory);

private:
    std::string test_data_directory_;
    int simulation_delay_ms_;
    int connection_delay_ms_;
    
    std::atomic<bool> connected_;
    std::atomic<websocket_transport::WebSocketState> state_;
    std::atomic<bool> loop_running_;
    
    // Callbacks
    websocket_transport::WebSocketMessageCallback message_callback_;
    websocket_transport::WebSocketErrorCallback error_callback_;
    websocket_transport::WebSocketConnectCallback connect_callback_;
    
    // Configuration
    std::atomic<int> ping_interval_;
    std::atomic<int> timeout_;
    std::atomic<int> reconnect_attempts_;
    std::atomic<int> reconnect_delay_;
    
    // Simulation
    std::thread simulation_thread_;
    std::atomic<bool> simulation_running_;
    std::queue<std::string> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_cv_;
    
    // Internal methods
    void simulation_loop();
    void process_message_queue();
    void send_message_to_callback(const std::string& message);
    std::string load_json_file(const std::string& filename);
    void queue_message(const std::string& message);
};

// Test factory for creating mock transports
class TestWebSocketTransportFactory {
public:
    static std::unique_ptr<websocket_transport::IWebSocketTransport> create_mock();
    static std::unique_ptr<websocket_transport::IWebSocketTransport> create_mock_with_data(const std::string& test_data_directory);
    static MockWebSocketTransport* cast_to_mock(websocket_transport::IWebSocketTransport* transport);
};

} // namespace test_utils
