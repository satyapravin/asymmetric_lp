#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace websocket_transport {

// WebSocket message structure
struct WebSocketMessage {
    std::string data;
    bool is_binary;
    uint64_t timestamp_us;
    std::string channel;
};

// WebSocket connection states
enum class WebSocketState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    ERROR
};

// Callback types
using WebSocketMessageCallback = std::function<void(const WebSocketMessage& message)>;
using WebSocketErrorCallback = std::function<void(int error_code, const std::string& error_message)>;
using WebSocketConnectCallback = std::function<void(bool connected)>;

// WebSocket Transport Interface
class IWebSocketTransport {
public:
    virtual ~IWebSocketTransport() = default;
    
    // Connection management
    virtual bool connect(const std::string& url) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual WebSocketState get_state() const = 0;
    
    // Message handling
    virtual bool send_message(const std::string& message, bool binary = false) = 0;
    virtual bool send_binary(const std::vector<uint8_t>& data) = 0;
    virtual bool send_ping() = 0;
    
    // Callbacks
    virtual void set_message_callback(WebSocketMessageCallback callback) = 0;
    virtual void set_error_callback(WebSocketErrorCallback callback) = 0;
    virtual void set_connect_callback(WebSocketConnectCallback callback) = 0;
    
    // Configuration
    virtual void set_ping_interval(int seconds) = 0;
    virtual void set_timeout(int seconds) = 0;
    virtual void set_reconnect_attempts(int attempts) = 0;
    virtual void set_reconnect_delay(int seconds) = 0;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Event loop management
    virtual void start_event_loop() = 0;
    virtual void stop_event_loop() = 0;
    virtual bool is_event_loop_running() const = 0;
};

} // namespace websocket_transport
