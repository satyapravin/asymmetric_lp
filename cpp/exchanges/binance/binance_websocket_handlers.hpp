#pragma once
#include "i_exchange_websocket_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace binance {

// Binance WebSocket message types
enum class BinanceMessageType {
    ORDER_UPDATE,
    ACCOUNT_UPDATE,
    BALANCE_UPDATE,
    POSITION_UPDATE,
    TRADE_UPDATE,
    MARKET_DATA,
    ERROR_MESSAGE
};

// Binance WebSocket message structure
struct BinanceWebSocketMessage {
    BinanceMessageType type;
    std::string data;
    std::string symbol;
    std::string order_id;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// Binance WebSocket callback types
using BinanceMessageCallback = std::function<void(const BinanceWebSocketMessage& message)>;
using BinanceOrderCallback = std::function<void(const std::string& order_id, const std::string& status)>;
using BinanceTradeCallback = std::function<void(const std::string& trade_id, double qty, double price)>;

// Binance Public WebSocket Handler (Market Data)
class BinancePublicWebSocketHandler : public IExchangeWebSocketHandler {
public:
    BinancePublicWebSocketHandler();
    ~BinancePublicWebSocketHandler();
    
    // IWebSocketHandler interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    WebSocketState get_state() const override;
    
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    
    void set_message_callback(WebSocketMessageCallback callback) override;
    void set_error_callback(WebSocketErrorCallback callback) override;
    void set_connect_callback(WebSocketConnectCallback callback) override;
    
    void set_ping_interval(int seconds) override;
    void set_timeout(int seconds) override;
    void set_reconnect_attempts(int attempts) override;
    void set_reconnect_delay(int seconds) override;
    
    bool initialize() override;
    void shutdown() override;
    
    WebSocketType get_type() const override { return WebSocketType::PUBLIC_MARKET_DATA; }
    std::string get_channel() const override { return "binance_public"; }
    
    // IExchangeWebSocketHandler interface
    std::string get_exchange_name() const override { return "BINANCE"; }
    bool subscribe_to_channel(const std::string& channel) override;
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override { return false; } // Public streams don't need auth
    
    // Binance-specific methods
    bool subscribe_to_ticker(const std::string& symbol);
    bool subscribe_to_depth(const std::string& symbol, int levels = 20);
    bool subscribe_to_trades(const std::string& symbol);
    bool subscribe_to_kline(const std::string& symbol, const std::string& interval);

private:
    void handle_message(const std::string& message);
    void handle_ping();
    void send_pong();
    
    std::atomic<bool> connected_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    std::vector<std::string> subscribed_channels_;
    std::mutex channels_mutex_;
    
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    
    std::thread worker_thread_;
    std::atomic<bool> should_stop_{false};
};

// Binance Private WebSocket Handler (User Data)
class BinancePrivateWebSocketHandler : public IExchangeWebSocketHandler {
public:
    BinancePrivateWebSocketHandler();
    ~BinancePrivateWebSocketHandler();
    
    // IWebSocketHandler interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    WebSocketState get_state() const override;
    
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    
    void set_message_callback(WebSocketMessageCallback callback) override;
    void set_error_callback(WebSocketErrorCallback callback) override;
    void set_connect_callback(WebSocketConnectCallback callback) override;
    
    void set_ping_interval(int seconds) override;
    void set_timeout(int seconds) override;
    void set_reconnect_attempts(int attempts) override;
    void set_reconnect_delay(int seconds) override;
    
    bool initialize() override;
    void shutdown() override;
    
    WebSocketType get_type() const override { return WebSocketType::PRIVATE_USER_DATA; }
    std::string get_channel() const override { return "binance_private"; }
    
    // IExchangeWebSocketHandler interface
    std::string get_exchange_name() const override { return "BINANCE"; }
    bool subscribe_to_channel(const std::string& channel) override;
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Binance-specific methods
    bool subscribe_to_user_data();
    bool subscribe_to_account_updates();
    bool subscribe_to_order_updates();
    bool subscribe_to_trade_updates();
    
    // Callbacks for specific events
    void set_order_callback(BinanceOrderCallback callback);
    void set_trade_callback(BinanceTradeCallback callback);
    void set_message_callback(BinanceMessageCallback callback);

private:
    void handle_message(const std::string& message);
    void handle_user_data_message(const std::string& message);
    void handle_order_update(const std::string& message);
    void handle_trade_update(const std::string& message);
    void handle_account_update(const std::string& message);
    
    std::string generate_listen_key();
    void refresh_listen_key();
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    std::vector<std::string> subscribed_channels_;
    std::mutex channels_mutex_;
    
    std::string api_key_;
    std::string api_secret_;
    std::string listen_key_;
    std::chrono::steady_clock::time_point listen_key_expiry_;
    
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    BinanceOrderCallback order_callback_;
    BinanceTradeCallback trade_callback_;
    BinanceMessageCallback binance_message_callback_;
    
    std::thread worker_thread_;
    std::thread listen_key_refresh_thread_;
    std::atomic<bool> should_stop_{false};
};

// Binance WebSocket Manager - manages all WebSocket connections
class BinanceWebSocketManager {
public:
    BinanceWebSocketManager();
    ~BinanceWebSocketManager();
    
    // Initialize all WebSocket connections
    bool initialize(const std::string& api_key, const std::string& api_secret);
    void shutdown();
    
    // Get specific handlers
    std::shared_ptr<BinancePublicWebSocketHandler> get_public_handler();
    std::shared_ptr<BinancePrivateWebSocketHandler> get_private_handler();
    
    // Connection management
    bool connect_all();
    void disconnect_all();
    bool is_connected() const;
    
    // Callbacks
    void set_order_callback(BinanceOrderCallback callback);
    void set_trade_callback(BinanceTradeCallback callback);
    void set_market_data_callback(WebSocketMessageCallback callback);
    
    // Subscription management
    bool subscribe_to_ticker(const std::string& symbol);
    bool subscribe_to_depth(const std::string& symbol, int levels = 20);
    bool subscribe_to_user_data();

private:
    std::shared_ptr<BinancePublicWebSocketHandler> public_handler_;
    std::shared_ptr<BinancePrivateWebSocketHandler> private_handler_;
    
    std::string api_key_;
    std::string api_secret_;
    std::atomic<bool> initialized_{false};
};

} // namespace binance
