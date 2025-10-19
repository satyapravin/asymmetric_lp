#pragma once
#include "../../websocket/i_exchange_websocket_handler.hpp"
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

namespace binance {

// Binance Public WebSocket message types (Market Data)
enum class BinancePublicMessageType {
    MARKET_DATA,
    ORDERBOOK_UPDATE,
    TICKER_UPDATE,
    TRADE_UPDATE,
    KLINE_UPDATE,
    ERROR_MESSAGE
};

// Binance Public WebSocket message structure
struct BinancePublicWebSocketMessage {
    BinancePublicMessageType type;
    std::string data;
    std::string symbol;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// Binance Public WebSocket callback types
using BinancePublicMessageCallback = std::function<void(const BinancePublicWebSocketMessage& message)>;
using BinanceOrderbookCallback = std::function<void(const std::string& symbol, const std::vector<std::pair<double, double>>& bids, const std::vector<std::pair<double, double>>& asks)>;
using BinanceTickerCallback = std::function<void(const std::string& symbol, double price, double volume)>;
using BinanceTradeCallback = std::function<void(const std::string& symbol, double price, double qty)>;

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
    bool subscribe_to_channel(const std::string& channel) override;
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override { return true; } // Public streams don't require explicit auth
    WebSocketType get_type() const override { return WebSocketType::PUBLIC_MARKET_DATA; }
    std::string get_exchange_name() const override { return "BINANCE"; }
    std::string get_channel() const override { return "public"; }

    // Binance-specific public subscriptions
    bool subscribe_to_orderbook(const std::string& symbol);
    bool subscribe_to_trades(const std::string& symbol);
    bool subscribe_to_ticker(const std::string& symbol);

    // Message handling
    void handle_message(const std::string& message);

private:
    std::atomic<bool> connected_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<bool> should_stop_{false};
    
    // WebSocket connection management
    std::string websocket_url_;
    std::atomic<int> ping_interval_{30};
    std::atomic<int> timeout_{30};
    std::atomic<int> reconnect_attempts_{5};
    std::atomic<int> reconnect_delay_{5};
    
    // Connection thread
    std::thread connection_thread_;
    std::atomic<bool> connection_thread_running_{false};
    
    void connection_loop();
    void handle_websocket_message(const std::string& message);
};

} // namespace binance