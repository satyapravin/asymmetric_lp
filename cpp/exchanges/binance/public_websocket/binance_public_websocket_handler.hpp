#pragma once
#include "../../websocket/i_websocket_transport.hpp"
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
#include <json/json.h>

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

// Binance Public WebSocket Handler (Market Data) - Uses transport abstraction
class BinancePublicWebSocketHandler : public IExchangeWebSocketHandler {
public:
    BinancePublicWebSocketHandler();
    explicit BinancePublicWebSocketHandler(std::unique_ptr<websocket_transport::IWebSocketTransport> transport);
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
    std::string get_channel() const override { return "public"; }

    // IExchangeWebSocketHandler interface
    std::string get_exchange_name() const override { return "BINANCE"; }
    bool subscribe_to_channel(const std::string& channel) override;
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override { return true; } // Public streams don't require explicit auth

    // Binance-specific subscriptions
    bool subscribe_to_orderbook(const std::string& symbol);
    bool subscribe_to_trades(const std::string& symbol);
    bool subscribe_to_ticker(const std::string& symbol);

    // Transport configuration
    void set_transport_type(const std::string& transport_type);
    void configure_mock_transport(const std::string& test_data_directory, 
                                int simulation_delay_ms = 10,
                                int connection_delay_ms = 50);

private:
    // Transport abstraction
    std::unique_ptr<websocket_transport::IWebSocketTransport> transport_;
    
    // Channel management
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    
    // Callbacks
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    
    // Internal methods
    void handle_websocket_message(const websocket_transport::WebSocketMessage& message);
    void handle_connection_error(int error_code, const std::string& error_message);
    void handle_connection_status(bool connected);
    
    // Message parsing
    void parse_binance_message(const std::string& message);
    void handle_orderbook_update(const Json::Value& data);
    void handle_trade_update(const Json::Value& data);
    void handle_ticker_update(const Json::Value& data);
};

} // namespace binance