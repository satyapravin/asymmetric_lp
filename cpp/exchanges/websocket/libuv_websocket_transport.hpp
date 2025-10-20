#pragma once
#include "i_websocket_transport.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

// Include libuv headers directly
#include <uv.h>

namespace websocket_transport {

// Real libuv implementation
class LibuvWebSocketTransport : public IWebSocketTransport {
public:
    LibuvWebSocketTransport();
    ~LibuvWebSocketTransport();
    
    // IWebSocketTransport interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    WebSocketState get_state() const override;
    
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    bool send_ping() override;
    
    void set_message_callback(WebSocketMessageCallback callback) override;
    void set_error_callback(WebSocketErrorCallback callback) override;
    void set_connect_callback(WebSocketConnectCallback callback) override;
    
    void set_ping_interval(int seconds) override;
    void set_timeout(int seconds) override;
    void set_reconnect_attempts(int attempts) override;
    void set_reconnect_delay(int seconds) override;
    
    bool initialize() override;
    void shutdown() override;
    
    void start_event_loop() override;
    void stop_event_loop() override;
    bool is_event_loop_running() const override;

private:
    // libuv components
    uv_loop_t* loop_;
    uv_async_t* async_handle_;
    uv_timer_t* ping_timer_;
    uv_timer_t* reconnect_timer_;
    uv_tcp_t* tcp_handle_;
    
    // WebSocket connection
    std::string websocket_url_;
    std::atomic<bool> connected_;
    std::atomic<WebSocketState> state_;
    
    // Callbacks
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    
    // Configuration
    std::atomic<int> ping_interval_;
    std::atomic<int> timeout_;
    std::atomic<int> reconnect_attempts_;
    std::atomic<int> reconnect_delay_;
    std::atomic<int> current_reconnect_attempts_;
    
    // Event loop management
    std::thread event_loop_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> loop_running_;
    
    // Message queue for thread-safe communication
    std::queue<std::string> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_cv_;
    
    // libuv callbacks
    static void on_tcp_connect(uv_connect_t* req, int status);
    static void on_tcp_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void on_tcp_write(uv_write_t* req, int status);
    static void on_ping_timer(uv_timer_t* timer);
    static void on_reconnect_timer(uv_timer_t* timer);
    static void on_async_callback(uv_async_t* handle);
    
    // Internal methods
    void event_loop_thread_func();
    void handle_websocket_message(const std::string& message);
    void handle_connection_error(const std::string& error);
    void schedule_reconnect();
    void process_message_queue();
};

} // namespace websocket_transport
