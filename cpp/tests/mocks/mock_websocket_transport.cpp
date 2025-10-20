#include "mock_websocket_transport.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace test_utils {

MockWebSocketTransport::MockWebSocketTransport() {
    std::cout << "[MOCK_TRANSPORT] Initializing mock WebSocket transport" << std::endl;
    
    connected_.store(false);
    state_.store(websocket_transport::WebSocketState::DISCONNECTED);
    loop_running_.store(false);
    simulation_running_.store(false);
    
    // Default configuration
    simulation_delay_ms_ = 10;
    connection_delay_ms_ = 50;
    ping_interval_.store(30);
    timeout_.store(30);
    reconnect_attempts_.store(3);
    reconnect_delay_.store(5);
    
    std::cout << "[MOCK_TRANSPORT] Mock transport initialized" << std::endl;
}

MockWebSocketTransport::~MockWebSocketTransport() {
    shutdown();
}

bool MockWebSocketTransport::connect(const std::string& url) {
    std::cout << "[MOCK_TRANSPORT] Mock connecting to: " << url << std::endl;
    
    state_.store(websocket_transport::WebSocketState::CONNECTING);
    
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(connection_delay_ms_));
    
    // Simulate successful connection
    state_.store(websocket_transport::WebSocketState::CONNECTED);
    connected_.store(true);
    
    if (connect_callback_) {
        connect_callback_(true);
    }
    
    std::cout << "[MOCK_TRANSPORT] Mock connected successfully" << std::endl;
    return true;
}

void MockWebSocketTransport::disconnect() {
    std::cout << "[MOCK_TRANSPORT] Mock disconnecting" << std::endl;
    
    state_.store(websocket_transport::WebSocketState::DISCONNECTING);
    connected_.store(false);
    
    if (connect_callback_) {
        connect_callback_(false);
    }
    
    state_.store(websocket_transport::WebSocketState::DISCONNECTED);
    
    std::cout << "[MOCK_TRANSPORT] Mock disconnected" << std::endl;
}

bool MockWebSocketTransport::is_connected() const {
    return connected_.load();
}

websocket_transport::WebSocketState MockWebSocketTransport::get_state() const {
    return state_.load();
}

bool MockWebSocketTransport::send_message(const std::string& message, bool binary) {
    if (!is_connected()) {
        std::cerr << "[MOCK_TRANSPORT] Cannot send message: not connected" << std::endl;
        return false;
    }
    
    std::cout << "[MOCK_TRANSPORT] Mock sending message: " << message << std::endl;
    return true;
}

bool MockWebSocketTransport::send_binary(const std::vector<uint8_t>& data) {
    std::string message(data.begin(), data.end());
    return send_message(message, true);
}

bool MockWebSocketTransport::send_ping() {
    return send_message("ping", false);
}

void MockWebSocketTransport::set_message_callback(websocket_transport::WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void MockWebSocketTransport::set_error_callback(websocket_transport::WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void MockWebSocketTransport::set_connect_callback(websocket_transport::WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void MockWebSocketTransport::set_ping_interval(int seconds) {
    ping_interval_.store(seconds);
}

void MockWebSocketTransport::set_timeout(int seconds) {
    timeout_.store(seconds);
}

void MockWebSocketTransport::set_reconnect_attempts(int attempts) {
    reconnect_attempts_.store(attempts);
}

void MockWebSocketTransport::set_reconnect_delay(int seconds) {
    reconnect_delay_.store(seconds);
}

bool MockWebSocketTransport::initialize() {
    return true;
}

void MockWebSocketTransport::shutdown() {
    stop_event_loop();
}

void MockWebSocketTransport::start_event_loop() {
    if (!loop_running_.load()) {
        loop_running_.store(true);
        simulation_running_.store(true);
        simulation_thread_ = std::thread(&MockWebSocketTransport::simulation_loop, this);
    }
}

void MockWebSocketTransport::stop_event_loop() {
    simulation_running_.store(false);
    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }
    loop_running_.store(false);
}

bool MockWebSocketTransport::is_event_loop_running() const {
    return loop_running_.load();
}

// Mock-specific methods
void MockWebSocketTransport::set_test_data_directory(const std::string& directory) {
    test_data_directory_ = directory;
    std::cout << "[MOCK_TRANSPORT] Test data directory set to: " << directory << std::endl;
}

void MockWebSocketTransport::set_simulation_delay_ms(int delay_ms) {
    simulation_delay_ms_ = delay_ms;
    std::cout << "[MOCK_TRANSPORT] Simulation delay set to: " << delay_ms << "ms" << std::endl;
}

void MockWebSocketTransport::set_connection_delay_ms(int delay_ms) {
    connection_delay_ms_ = delay_ms;
    std::cout << "[MOCK_TRANSPORT] Connection delay set to: " << delay_ms << "ms" << std::endl;
}

void MockWebSocketTransport::simulate_orderbook_message(const std::string& symbol) {
    std::string filename = test_data_directory_ + "/orderbook_snapshot_message.json";
    load_and_replay_json_file(filename);
}

void MockWebSocketTransport::simulate_trade_message(const std::string& symbol) {
    std::string filename = test_data_directory_ + "/trade_message.json";
    load_and_replay_json_file(filename);
}

void MockWebSocketTransport::simulate_ticker_message(const std::string& symbol) {
    // No ticker fixture in tests; no-op
}

void MockWebSocketTransport::simulate_custom_message(const std::string& message) {
    queue_message(message);
}

void MockWebSocketTransport::simulate_connection_success() {
    state_.store(websocket_transport::WebSocketState::CONNECTED);
    connected_.store(true);
    
    if (connect_callback_) {
        connect_callback_(true);
    }
}

void MockWebSocketTransport::simulate_connection_failure() {
    state_.store(websocket_transport::WebSocketState::ERROR);
    connected_.store(false);
    
    if (error_callback_) {
        error_callback_(-1, "Mock connection failure");
    }
}

void MockWebSocketTransport::simulate_disconnection() {
    state_.store(websocket_transport::WebSocketState::DISCONNECTED);
    connected_.store(false);
    
    if (connect_callback_) {
        connect_callback_(false);
    }
}

void MockWebSocketTransport::simulate_error(int error_code, const std::string& error_message) {
    if (error_callback_) {
        error_callback_(error_code, error_message);
    }
}

void MockWebSocketTransport::load_and_replay_json_file(const std::string& filename) {
    std::string content = load_json_file(filename);
    if (!content.empty()) {
        queue_message(content);
    }
}

void MockWebSocketTransport::load_and_replay_json_directory(const std::string& directory) {
    std::cout << "[MOCK_TRANSPORT] Loading JSON files from directory: " << directory << std::endl;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().string();
                std::cout << "[MOCK_TRANSPORT] Loading file: " << filename << std::endl;
                load_and_replay_json_file(filename);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[MOCK_TRANSPORT] Error reading directory: " << e.what() << std::endl;
    }
}

// Internal methods
void MockWebSocketTransport::simulation_loop() {
    std::cout << "[MOCK_TRANSPORT] Starting simulation loop" << std::endl;
    
    while (simulation_running_.load()) {
        process_message_queue();
        std::this_thread::sleep_for(std::chrono::milliseconds(simulation_delay_ms_));
    }
    
    std::cout << "[MOCK_TRANSPORT] Simulation loop stopped" << std::endl;
}

void MockWebSocketTransport::process_message_queue() {
    std::lock_guard<std::mutex> lock(message_queue_mutex_);
    
    while (!message_queue_.empty()) {
        std::string message = message_queue_.front();
        message_queue_.pop();
        
        send_message_to_callback(message);
    }
}

void MockWebSocketTransport::send_message_to_callback(const std::string& message) {
    if (message_callback_) {
        websocket_transport::WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.is_binary = false;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_message.channel = "";
        
        message_callback_(ws_message);
    }
}

std::string MockWebSocketTransport::load_json_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[MOCK_TRANSPORT] Could not open file: " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    std::cout << "[MOCK_TRANSPORT] Loaded JSON file: " << filename << " (" << content.length() << " bytes)" << std::endl;
    return content;
}

void MockWebSocketTransport::queue_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(message_queue_mutex_);
    message_queue_.push(message);
}

// Test factory implementation
std::unique_ptr<websocket_transport::IWebSocketTransport> TestWebSocketTransportFactory::create_mock() {
    return std::make_unique<MockWebSocketTransport>();
}

std::unique_ptr<websocket_transport::IWebSocketTransport> TestWebSocketTransportFactory::create_mock_with_data(const std::string& test_data_directory) {
    auto mock_transport = std::make_unique<MockWebSocketTransport>();
    mock_transport->set_test_data_directory(test_data_directory);
    return mock_transport;
}

MockWebSocketTransport* TestWebSocketTransportFactory::cast_to_mock(websocket_transport::IWebSocketTransport* transport) {
    return dynamic_cast<MockWebSocketTransport*>(transport);
}

} // namespace test_utils
