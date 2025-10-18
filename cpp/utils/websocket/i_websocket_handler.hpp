#pragma once
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>

// WebSocket message structure
struct WebSocketMessage {
    std::string data;
    bool is_binary{false};
    uint64_t timestamp_us{0};
};

// WebSocket event callbacks
using WebSocketMessageCallback = std::function<void(const WebSocketMessage& message)>;
using WebSocketErrorCallback = std::function<void(const std::string& error)>;
using WebSocketConnectCallback = std::function<void(bool connected)>;

// WebSocket connection states
enum class WebSocketState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    ERROR
};

// Base interface for WebSocket handlers
class IWebSocketHandler {
public:
    virtual ~IWebSocketHandler() = default;
    
    // Connection management
    virtual bool connect(const std::string& url) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual WebSocketState get_state() const = 0;
    
    // Message handling
    virtual bool send_message(const std::string& message, bool binary = false) = 0;
    virtual bool send_binary(const std::vector<uint8_t>& data) = 0;
    
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
};

// WebSocket handler factory
class WebSocketHandlerFactory {
public:
    enum class Type {
        LIBUV,
        WEBSOCKETPP,
        CUSTOM
    };
    
    static std::unique_ptr<IWebSocketHandler> create(Type type = Type::LIBUV);
    static std::unique_ptr<IWebSocketHandler> create(const std::string& type_name);
};
