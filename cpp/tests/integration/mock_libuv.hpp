#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <cstdint>
#include <fstream>
#include <json/json.h>

namespace mock_libuv {

// Mock WebSocket types to match libuv interface
enum class WebSocketState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    ERROR
};

enum class WebSocketType {
    PUBLIC_MARKET_DATA,
    PRIVATE_ACCOUNT_DATA
};

struct WebSocketMessage {
    std::string data;
    bool is_binary;
    uint64_t timestamp_us;
    std::string channel;
};

// Mock callback types
using WebSocketMessageCallback = std::function<void(const WebSocketMessage& message)>;
using WebSocketErrorCallback = std::function<void(int error_code, const std::string& error_message)>;
using WebSocketConnectCallback = std::function<void(bool connected)>;

// Mock WebSocket connection structure
struct websockets_connection_t {
    void* data;
    std::string url;
    bool connected;
    WebSocketMessageCallback message_callback;
    WebSocketErrorCallback error_callback;
    WebSocketConnectCallback connect_callback;
};

// Mock WebSocket structure
struct websockets {
    websockets_connection_t* connection;
    std::string url;
    bool connected;
};

// Mock libuv functions
int websockets_client_connect(const char* url,
                             WebSocketConnectCallback on_open,
                             WebSocketMessageCallback on_message,
                             std::function<void(websockets_connection_t*, int, const char*)> on_close,
                             WebSocketErrorCallback on_error,
                             void* user_data);

int websockets_send_text(struct websockets* ws, const char* data, size_t length);
int websockets_send_binary(struct websockets* ws, const void* data, size_t length);
int websockets_send_ping(struct websockets* ws);
void websockets_close(struct websockets* ws);

// Mock libuv loop functions
struct uv_loop_t {
    bool running;
};

struct uv_async_t {
    void* data;
    std::function<void(uv_async_t*)> callback;
};

struct uv_timer_t {
    void* data;
    std::function<void(uv_timer_t*)> callback;
    bool active;
    uint64_t timeout;
    uint64_t repeat;
};

// Mock libuv functions
uv_loop_t* uv_default_loop();
int uv_async_init(uv_loop_t* loop, uv_async_t* handle, std::function<void(uv_async_t*)> callback);
int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle);
int uv_timer_start(uv_timer_t* handle, std::function<void(uv_timer_t*)> callback, uint64_t timeout, uint64_t repeat);
int uv_timer_stop(uv_timer_t* handle);
int uv_async_send(uv_async_t* handle);
int uv_run(uv_loop_t* loop, int mode);

// Mock WebSocket Handler for testing
class MockWebSocketHandler {
public:
    MockWebSocketHandler();
    ~MockWebSocketHandler();
    
    // Configuration
    void set_test_data_directory(const std::string& directory);
    void set_simulation_delay_ms(int delay_ms);
    void set_connection_delay_ms(int delay_ms);
    
    // Message simulation
    void simulate_orderbook_message(const std::string& symbol);
    void simulate_trade_message(const std::string& symbol);
    void simulate_ticker_message(const std::string& symbol);
    void simulate_custom_message(const std::string& message);
    
    // Connection simulation
    void simulate_connection_success();
    void simulate_connection_failure();
    void simulate_disconnection();
    void simulate_error(int error_code, const std::string& error_message);
    
    // Message sequence simulation
    void simulate_message_sequence(const std::vector<std::string>& messages);
    void load_message_sequence_from_file(const std::string& filename);
    
private:
    std::string test_data_directory_;
    int simulation_delay_ms_;
    int connection_delay_ms_;
    
    std::thread simulation_thread_;
    std::atomic<bool> simulation_running_;
    std::queue<std::string> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_cv_;
    
    // Current connection state
    std::atomic<bool> connected_;
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    
    // Internal methods
    void simulation_loop();
    void process_message_queue();
    std::string load_json_file(const std::string& filename);
    void send_message_to_callback(const std::string& message);
};

// Global mock handler instance
extern std::unique_ptr<MockWebSocketHandler> g_mock_handler;

// Mock initialization functions
void initialize_mock_libuv();
void shutdown_mock_libuv();
void set_mock_test_data_directory(const std::string& directory);

} // namespace mock_libuv
