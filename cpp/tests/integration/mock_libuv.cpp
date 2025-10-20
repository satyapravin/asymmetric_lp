#include "mock_libuv.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <random>

namespace mock_libuv {

// Global mock handler instance
std::unique_ptr<MockWebSocketHandler> g_mock_handler = nullptr;

// Mock WebSocket functions
int websockets_client_connect(const char* url,
                             WebSocketConnectCallback on_open,
                             WebSocketMessageCallback on_message,
                             std::function<void(websockets_connection_t*, int, const char*)> on_close,
                             WebSocketErrorCallback on_error,
                             void* user_data) {
    
    if (!g_mock_handler) {
        std::cerr << "[MOCK_LIBUV] Mock handler not initialized" << std::endl;
        return -1;
    }
    
    std::cout << "[MOCK_LIBUV] Connecting to: " << url << std::endl;
    
    // Create mock connection
    websockets_connection_t* connection = new websockets_connection_t();
    connection->data = user_data;
    connection->url = url;
    connection->connected = false;
    connection->message_callback = on_message;
    connection->error_callback = on_error;
    connection->connect_callback = on_open;
    
    // Create mock WebSocket
    websockets* ws = new websockets();
    ws->connection = connection;
    ws->url = url;
    ws->connected = false;
    
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(g_mock_handler->get_connection_delay_ms()));
    
    // Simulate successful connection
    connection->connected = true;
    ws->connected = true;
    
    if (on_open) {
        on_open(connection);
    }
    
    std::cout << "[MOCK_LIBUV] Connection established" << std::endl;
    return 0;
}

int websockets_send_text(struct websockets* ws, const char* data, size_t length) {
    if (!ws || !ws->connected) {
        std::cerr << "[MOCK_LIBUV] Cannot send text: not connected" << std::endl;
        return -1;
    }
    
    std::string message(data, length);
    std::cout << "[MOCK_LIBUV] Sending text: " << message << std::endl;
    
    // In a real implementation, this would send over WebSocket
    // For testing, we just log it
    return 0;
}

int websockets_send_binary(struct websockets* ws, const void* data, size_t length) {
    if (!ws || !ws->connected) {
        std::cerr << "[MOCK_LIBUV] Cannot send binary: not connected" << std::endl;
        return -1;
    }
    
    std::cout << "[MOCK_LIBUV] Sending binary: " << length << " bytes" << std::endl;
    
    // In a real implementation, this would send over WebSocket
    // For testing, we just log it
    return 0;
}

int websockets_send_ping(struct websockets* ws) {
    if (!ws || !ws->connected) {
        std::cerr << "[MOCK_LIBUV] Cannot send ping: not connected" << std::endl;
        return -1;
    }
    
    std::cout << "[MOCK_LIBUV] Sending ping" << std::endl;
    
    // In a real implementation, this would send ping over WebSocket
    // For testing, we just log it
    return 0;
}

void websockets_close(struct websockets* ws) {
    if (!ws) {
        return;
    }
    
    std::cout << "[MOCK_LIBUV] Closing WebSocket connection" << std::endl;
    
    if (ws->connection) {
        ws->connection->connected = false;
        // In a real implementation, this would trigger on_close callback
    }
    
    ws->connected = false;
    
    // Clean up
    delete ws->connection;
    delete ws;
}

// Mock libuv functions
uv_loop_t* uv_default_loop() {
    static uv_loop_t loop;
    loop.running = true;
    return &loop;
}

int uv_async_init(uv_loop_t* loop, uv_async_t* handle, std::function<void(uv_async_t*)> callback) {
    handle->data = nullptr;
    handle->callback = callback;
    return 0;
}

int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle) {
    handle->data = nullptr;
    handle->callback = nullptr;
    handle->active = false;
    handle->timeout = 0;
    handle->repeat = 0;
    return 0;
}

int uv_timer_start(uv_timer_t* handle, std::function<void(uv_timer_t*)> callback, uint64_t timeout, uint64_t repeat) {
    handle->callback = callback;
    handle->active = true;
    handle->timeout = timeout;
    handle->repeat = repeat;
    
    // In a real implementation, this would start a timer
    // For testing, we just mark it as active
    std::cout << "[MOCK_LIBUV] Timer started: timeout=" << timeout << "ms, repeat=" << repeat << "ms" << std::endl;
    return 0;
}

int uv_timer_stop(uv_timer_t* handle) {
    handle->active = false;
    std::cout << "[MOCK_LIBUV] Timer stopped" << std::endl;
    return 0;
}

int uv_async_send(uv_async_t* handle) {
    if (handle->callback) {
        // In a real implementation, this would trigger the callback in the event loop
        // For testing, we call it immediately
        handle->callback(handle);
    }
    return 0;
}

int uv_run(uv_loop_t* loop, int mode) {
    // In a real implementation, this would run the event loop
    // For testing, we just return immediately
    return 0;
}

// MockWebSocketHandler implementation
MockWebSocketHandler::MockWebSocketHandler() 
    : simulation_delay_ms_(10)
    , connection_delay_ms_(50)
    , simulation_running_(false) {
    
    std::cout << "[MOCK_LIBUV] MockWebSocketHandler initialized" << std::endl;
}

MockWebSocketHandler::~MockWebSocketHandler() {
    shutdown_mock_libuv();
}

void MockWebSocketHandler::set_test_data_directory(const std::string& directory) {
    test_data_directory_ = directory;
    std::cout << "[MOCK_LIBUV] Test data directory set to: " << directory << std::endl;
}

void MockWebSocketHandler::set_simulation_delay_ms(int delay_ms) {
    simulation_delay_ms_ = delay_ms;
    std::cout << "[MOCK_LIBUV] Simulation delay set to: " << delay_ms << "ms" << std::endl;
}

void MockWebSocketHandler::set_connection_delay_ms(int delay_ms) {
    connection_delay_ms_ = delay_ms;
    std::cout << "[MOCK_LIBUV] Connection delay set to: " << delay_ms << "ms" << std::endl;
}

int MockWebSocketHandler::get_connection_delay_ms() const {
    return connection_delay_ms_;
}

void MockWebSocketHandler::simulate_orderbook_message(const std::string& symbol) {
    std::string filename = test_data_directory_ + "/binance/websocket/orderbook_snapshot_message.json";
    std::string message = load_json_file(filename);
    
    if (!message.empty()) {
        // Replace placeholder symbol if needed
        if (message.find("BTCUSDT") != std::string::npos) {
            // Replace with actual symbol
            size_t pos = message.find("BTCUSDT");
            while (pos != std::string::npos) {
                message.replace(pos, 7, symbol);
                pos = message.find("BTCUSDT", pos + symbol.length());
            }
        }
        
        queue_message(message);
    }
}

void MockWebSocketHandler::simulate_trade_message(const std::string& symbol) {
    std::string filename = test_data_directory_ + "/binance/websocket/trade_message.json";
    std::string message = load_json_file(filename);
    
    if (!message.empty()) {
        // Replace placeholder symbol if needed
        if (message.find("BTCUSDT") != std::string::npos) {
            size_t pos = message.find("BTCUSDT");
            while (pos != std::string::npos) {
                message.replace(pos, 7, symbol);
                pos = message.find("BTCUSDT", pos + symbol.length());
            }
        }
        
        queue_message(message);
    }
}

void MockWebSocketHandler::simulate_ticker_message(const std::string& symbol) {
    std::string filename = test_data_directory_ + "/binance/websocket/ticker_message.json";
    std::string message = load_json_file(filename);
    
    if (!message.empty()) {
        // Replace placeholder symbol if needed
        if (message.find("BTCUSDT") != std::string::npos) {
            size_t pos = message.find("BTCUSDT");
            while (pos != std::string::npos) {
                message.replace(pos, 7, symbol);
                pos = message.find("BTCUSDT", pos + symbol.length());
            }
        }
        
        queue_message(message);
    }
}

void MockWebSocketHandler::simulate_custom_message(const std::string& message) {
    queue_message(message);
}

void MockWebSocketHandler::simulate_connection_success() {
    std::cout << "[MOCK_LIBUV] Simulating connection success" << std::endl;
    // This would trigger the on_open callback
}

void MockWebSocketHandler::simulate_connection_failure() {
    std::cout << "[MOCK_LIBUV] Simulating connection failure" << std::endl;
    // This would trigger the on_error callback
}

void MockWebSocketHandler::simulate_disconnection() {
    std::cout << "[MOCK_LIBUV] Simulating disconnection" << std::endl;
    // This would trigger the on_close callback
}

void MockWebSocketHandler::simulate_error(int error_code, const std::string& error_message) {
    std::cout << "[MOCK_LIBUV] Simulating error: " << error_code << " - " << error_message << std::endl;
    // This would trigger the on_error callback
}

void MockWebSocketHandler::simulate_message_sequence(const std::vector<std::string>& messages) {
    for (const auto& message : messages) {
        queue_message(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(simulation_delay_ms_));
    }
}

void MockWebSocketHandler::load_message_sequence_from_file(const std::string& filename) {
    std::string filepath = test_data_directory_ + "/" + filename;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "[MOCK_LIBUV] Failed to open message sequence file: " << filepath << std::endl;
        return;
    }
    
    std::string line;
    std::vector<std::string> messages;
    
    while (std::getline(file, line)) {
        if (!line.empty()) {
            messages.push_back(line);
        }
    }
    
    simulate_message_sequence(messages);
}

void MockWebSocketHandler::queue_message(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        message_queue_.push(message);
    }
    message_cv_.notify_one();
}

void MockWebSocketHandler::simulation_loop() {
    std::cout << "[MOCK_LIBUV] Starting simulation loop" << std::endl;
    
    while (simulation_running_.load()) {
        std::unique_lock<std::mutex> lock(message_queue_mutex_);
        
        if (message_queue_.empty()) {
            message_cv_.wait(lock, [this] { return !message_queue_.empty() || !simulation_running_.load(); });
        }
        
        if (!simulation_running_.load()) {
            break;
        }
        
        while (!message_queue_.empty()) {
            std::string message = message_queue_.front();
            message_queue_.pop();
            lock.unlock();
            
            send_message_to_callback(message);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(simulation_delay_ms_));
            
            lock.lock();
        }
    }
    
    std::cout << "[MOCK_LIBUV] Simulation loop stopped" << std::endl;
}

void MockWebSocketHandler::send_message_to_callback(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.is_binary = false;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_message.channel = "public";
        
        message_callback_(ws_message);
    }
}

std::string MockWebSocketHandler::load_json_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[MOCK_LIBUV] Failed to open file: " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    return content;
}

// Global mock functions
void initialize_mock_libuv() {
    if (!g_mock_handler) {
        g_mock_handler = std::make_unique<MockWebSocketHandler>();
        std::cout << "[MOCK_LIBUV] Mock libuv initialized" << std::endl;
    }
}

void shutdown_mock_libuv() {
    if (g_mock_handler) {
        g_mock_handler->simulation_running_.store(false);
        if (g_mock_handler->simulation_thread_.joinable()) {
            g_mock_handler->simulation_thread_.join();
        }
        g_mock_handler.reset();
        std::cout << "[MOCK_LIBUV] Mock libuv shutdown" << std::endl;
    }
}

void set_mock_test_data_directory(const std::string& directory) {
    if (g_mock_handler) {
        g_mock_handler->set_test_data_directory(directory);
    }
}

} // namespace mock_libuv
