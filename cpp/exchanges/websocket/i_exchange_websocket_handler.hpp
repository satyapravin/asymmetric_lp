#pragma once
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>
#include <map>

// WebSocket message structure
struct WebSocketMessage {
    std::string data;
    bool is_binary{false};
    uint64_t timestamp_us{0};
    std::string channel;  // For identifying message source
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

// WebSocket handler types
enum class WebSocketType {
    PUBLIC_MARKET_DATA,    // Public market data streams
    PRIVATE_USER_DATA,     // Private user data streams (orders, positions, etc.)
    PRIVATE_TRADING        // Private trading streams (order management)
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
    
    // Handler type
    virtual WebSocketType get_type() const = 0;
    virtual std::string get_channel() const = 0;
};

// Exchange-specific WebSocket handler interface
class IExchangeWebSocketHandler : public IWebSocketHandler {
public:
    virtual ~IExchangeWebSocketHandler() = default;
    
    // Exchange-specific methods
    virtual std::string get_exchange_name() const = 0;
    virtual bool subscribe_to_channel(const std::string& channel) = 0;
    virtual bool unsubscribe_from_channel(const std::string& channel) = 0;
    virtual std::vector<std::string> get_subscribed_channels() const = 0;
    
    // Authentication for private streams
    virtual void set_auth_credentials(const std::string& api_key, const std::string& secret) = 0;
    virtual bool is_authenticated() const = 0;
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
    static std::unique_ptr<IExchangeWebSocketHandler> create_exchange_handler(
        const std::string& exchange_name, 
        WebSocketType ws_type,
        Type implementation_type = Type::LIBUV);
};
