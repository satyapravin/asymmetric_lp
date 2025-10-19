#pragma once
#include "i_websocket_handler.hpp"
#include <uv.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <vector>

// libuv-based WebSocket handler implementation
class LibuvWebSocketHandler : public IWebSocketHandler {
public:
    LibuvWebSocketHandler();
    ~LibuvWebSocketHandler() override;
    
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override { return state_ == WebSocketState::CONNECTED; }
    WebSocketState get_state() const override { return state_; }
    
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    
    void set_message_callback(WebSocketMessageCallback callback) override { message_callback_ = callback; }
    void set_error_callback(WebSocketErrorCallback callback) override { error_callback_ = callback; }
    void set_connect_callback(WebSocketConnectCallback callback) override { connect_callback_ = callback; }
    
    void set_ping_interval(int seconds) override { ping_interval_seconds_ = seconds; }
    void set_timeout(int seconds) override { timeout_seconds_ = seconds; }
    void set_reconnect_attempts(int attempts) override { reconnect_attempts_ = attempts; }
    void set_reconnect_delay(int seconds) override { reconnect_delay_seconds_ = seconds; }
    
    bool initialize() override;
    void shutdown() override;

private:
    struct WebSocketData {
        uv_tcp_t tcp;
        uv_connect_t connect_req;
        uv_write_t write_req;
        uv_timer_t ping_timer;
        uv_timer_t reconnect_timer;
        LibuvWebSocketHandler* handler;
    };
    
    // libuv callbacks
    static void on_connect(uv_connect_t* req, int status);
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void on_write(uv_write_t* req, int status);
    static void on_ping_timer(uv_timer_t* timer);
    static void on_reconnect_timer(uv_timer_t* timer);
    static void on_close(uv_handle_t* handle);
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    
    // Internal methods
    void start_event_loop();
    void stop_event_loop();
    void handle_websocket_frame(const std::string& frame);
    void send_ping();
    void attempt_reconnect();
    void update_state(WebSocketState new_state);
    bool parse_url(const std::string& url, std::string& host, int& port, std::string& path, bool& ssl);
    bool perform_websocket_handshake();
    std::string generate_websocket_key();
    std::string base64_encode(const std::vector<uint8_t>& data);
    void process_received_data(const std::string& data);
    void send_pong(const std::string& payload);
    std::vector<uint8_t> serialize_frame(const WebSocketFrame& frame);
    
    // Configuration
    std::string url_;
    std::string host_;
    int port_;
    std::string path_;
    bool ssl_;
    int ping_interval_seconds_{30};
    int timeout_seconds_{60};
    int reconnect_attempts_{5};
    int reconnect_delay_seconds_{5};
    
    // State management
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    std::atomic<bool> running_{false};
    std::atomic<int> reconnect_count_{0};
    
    // libuv objects
    uv_loop_t* loop_{nullptr};
    std::unique_ptr<WebSocketData> ws_data_;
    std::thread event_loop_thread_;
    
    // Callbacks
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    
    // Thread safety
    std::mutex send_mutex_;
    std::queue<std::string> send_queue_;
};

// Factory implementation
inline std::unique_ptr<IWebSocketHandler> WebSocketHandlerFactory::create(WebSocketHandlerFactory::Type type) {
    switch (type) {
        case WebSocketHandlerFactory::Type::LIBUV:
            return std::make_unique<LibuvWebSocketHandler>();
        case WebSocketHandlerFactory::Type::WEBSOCKETPP:
            throw std::runtime_error("WebSocketPP handler not implemented yet");
        case WebSocketHandlerFactory::Type::CUSTOM:
            throw std::runtime_error("CUSTOM WebSocket handler not implemented yet");
        default:
            throw std::runtime_error("Unknown WebSocket handler type");
    }
}

inline std::unique_ptr<IWebSocketHandler> WebSocketHandlerFactory::create(const std::string& type_name) {
    if (type_name == "LIBUV" || type_name == "libuv") {
        return create(Type::LIBUV);
    } else if (type_name == "WEBSOCKETPP" || type_name == "websocketpp") {
        return create(Type::WEBSOCKETPP);
    } else if (type_name == "CUSTOM" || type_name == "custom") {
        return create(Type::CUSTOM);
    } else {
        throw std::runtime_error("Unknown WebSocket handler type: " + type_name);
    }
}
