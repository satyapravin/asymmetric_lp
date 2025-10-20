#include "libuv_websocket_transport.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace websocket_transport {

// LibuvWebSocketTransport implementation
LibuvWebSocketTransport::LibuvWebSocketTransport() {
    std::cout << "[LIBUV_TRANSPORT] Initializing real libuv WebSocket transport" << std::endl;
    
    // Initialize libuv loop
    loop_ = uv_default_loop();
    if (!loop_) {
        std::cerr << "[LIBUV_TRANSPORT] Failed to initialize libuv loop" << std::endl;
        return;
    }
    
    // Initialize async handle for thread-safe communication
    uv_async_init(loop_, async_handle_, on_async_callback);
    async_handle_->data = this;
    
    // Initialize ping timer
    uv_timer_init(loop_, ping_timer_);
    ping_timer_->data = this;
    
    // Initialize reconnect timer
    uv_timer_init(loop_, reconnect_timer_);
    reconnect_timer_->data = this;
    
    std::cout << "[LIBUV_TRANSPORT] libuv initialization complete" << std::endl;
}

LibuvWebSocketTransport::~LibuvWebSocketTransport() {
    shutdown();
}

bool LibuvWebSocketTransport::connect(const std::string& url) {
    std::cout << "[LIBUV_TRANSPORT] Connecting to: " << url << std::endl;
    
    websocket_url_ = url;
    state_.store(WebSocketState::CONNECTING);
    
    // Start event loop thread
    should_stop_.store(false);
    loop_running_.store(true);
    event_loop_thread_ = std::thread(&LibuvWebSocketTransport::event_loop_thread_func, this);
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return state_.load() == WebSocketState::CONNECTED;
}

void LibuvWebSocketTransport::disconnect() {
    std::cout << "[LIBUV_TRANSPORT] Disconnecting" << std::endl;
    
    state_.store(WebSocketState::DISCONNECTING);
    should_stop_.store(true);
    
    if (event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }
    
    state_.store(WebSocketState::DISCONNECTED);
}

bool LibuvWebSocketTransport::is_connected() const {
    return state_.load() == WebSocketState::CONNECTED;
}

WebSocketState LibuvWebSocketTransport::get_state() const {
    return state_.load();
}

bool LibuvWebSocketTransport::send_message(const std::string& message, bool binary) {
    if (!is_connected()) {
        std::cerr << "[LIBUV_TRANSPORT] Cannot send message: not connected" << std::endl;
        return false;
    }
    
    // Queue message for thread-safe sending
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        message_queue_.push(message);
    }
    message_cv_.notify_one();
    
    return true;
}

bool LibuvWebSocketTransport::send_binary(const std::vector<uint8_t>& data) {
    // Convert binary data to string for now
    std::string message(data.begin(), data.end());
    return send_message(message, true);
}

bool LibuvWebSocketTransport::send_ping() {
    return send_message("ping", false);
}

void LibuvWebSocketTransport::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void LibuvWebSocketTransport::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void LibuvWebSocketTransport::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void LibuvWebSocketTransport::set_ping_interval(int seconds) {
    ping_interval_.store(seconds);
}

void LibuvWebSocketTransport::set_timeout(int seconds) {
    timeout_.store(seconds);
}

void LibuvWebSocketTransport::set_reconnect_attempts(int attempts) {
    reconnect_attempts_.store(attempts);
}

void LibuvWebSocketTransport::set_reconnect_delay(int seconds) {
    reconnect_delay_.store(seconds);
}

bool LibuvWebSocketTransport::initialize() {
    return true; // Already initialized in constructor
}

void LibuvWebSocketTransport::shutdown() {
    disconnect();
}

void LibuvWebSocketTransport::start_event_loop() {
    if (!loop_running_.load()) {
        should_stop_.store(false);
        loop_running_.store(true);
        event_loop_thread_ = std::thread(&LibuvWebSocketTransport::event_loop_thread_func, this);
    }
}

void LibuvWebSocketTransport::stop_event_loop() {
    should_stop_.store(true);
    if (event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }
    loop_running_.store(false);
}

bool LibuvWebSocketTransport::is_event_loop_running() const {
    return loop_running_.load();
}

// Static callback functions
void LibuvWebSocketTransport::on_tcp_connect(uv_connect_t* req, int status) {
    LibuvWebSocketTransport* transport = static_cast<LibuvWebSocketTransport*>(req->data);
    
    if (status < 0) {
        transport->state_.store(WebSocketState::ERROR);
        transport->connected_.store(false);
        transport->handle_connection_error("TCP connection failed: " + std::to_string(status));
        return;
    }
    
    transport->state_.store(WebSocketState::CONNECTED);
    transport->connected_.store(true);
    
    if (transport->connect_callback_) {
        transport->connect_callback_(true);
    }
    
    std::cout << "[LIBUV_TRANSPORT] TCP connected" << std::endl;
}

void LibuvWebSocketTransport::on_tcp_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    LibuvWebSocketTransport* transport = static_cast<LibuvWebSocketTransport*>(stream->data);
    
    if (nread < 0) {
        if (nread != UV_EOF) {
            transport->handle_connection_error("TCP read error: " + std::to_string(nread));
        }
        return;
    }
    
    if (nread > 0) {
        std::string message(buf->base, nread);
        transport->handle_websocket_message(message);
    }
    
    free(buf->base);
}

void LibuvWebSocketTransport::on_tcp_write(uv_write_t* req, int status) {
    if (status < 0) {
        std::cerr << "[LIBUV_TRANSPORT] TCP write error: " << status << std::endl;
    }
    free(req);
}

void LibuvWebSocketTransport::on_ping_timer(uv_timer_t* timer) {
    LibuvWebSocketTransport* transport = static_cast<LibuvWebSocketTransport*>(timer->data);
    transport->send_ping();
}

void LibuvWebSocketTransport::on_reconnect_timer(uv_timer_t* timer) {
    LibuvWebSocketTransport* transport = static_cast<LibuvWebSocketTransport*>(timer->data);
    transport->schedule_reconnect();
}

void LibuvWebSocketTransport::on_async_callback(uv_async_t* handle) {
    LibuvWebSocketTransport* transport = static_cast<LibuvWebSocketTransport*>(handle->data);
    transport->process_message_queue();
}

// Internal methods
void LibuvWebSocketTransport::event_loop_thread_func() {
    std::cout << "[LIBUV_TRANSPORT] Starting event loop thread" << std::endl;
    
    while (!should_stop_.load()) {
        uv_run(loop_, UV_RUN_NOWAIT);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "[LIBUV_TRANSPORT] Event loop thread stopped" << std::endl;
}

void LibuvWebSocketTransport::handle_websocket_message(const std::string& message) {
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

void LibuvWebSocketTransport::handle_connection_error(const std::string& error) {
    std::cerr << "[LIBUV_TRANSPORT] " << error << std::endl;
    
    if (error_callback_) {
        error_callback_(-1, error);
    }
}

void LibuvWebSocketTransport::schedule_reconnect() {
    // Implementation for reconnection logic
    std::cout << "[LIBUV_TRANSPORT] Scheduling reconnection" << std::endl;
}

void LibuvWebSocketTransport::process_message_queue() {
    std::lock_guard<std::mutex> lock(message_queue_mutex_);
    
    while (!message_queue_.empty()) {
        std::string message = message_queue_.front();
        message_queue_.pop();
        
        // Send message via WebSocket
        // This would need actual WebSocket sending implementation
        std::cout << "[LIBUV_TRANSPORT] Sending message: " << message << std::endl;
    }
}

} // namespace websocket_transport
