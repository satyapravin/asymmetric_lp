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
    void send_message(const std::string& message) override;
    void send_binary(const std::vector<uint8_t>& data) override;
    
    // IExchangeWebSocketHandler interface
    WebSocketType get_type() const override { return WebSocketType::PUBLIC_MARKET_DATA; }
    std::string get_exchange_name() const override { return "BINANCE"; }
    
    // Public WebSocket specific methods
    bool subscribe_to_orderbook(const std::string& symbol, int depth = 20);
    bool subscribe_to_ticker(const std::string& symbol);
    bool subscribe_to_trades(const std::string& symbol);
    bool subscribe_to_kline(const std::string& symbol, const std::string& interval = "1m");
    bool unsubscribe_from_channel(const std::string& channel);
    
    // Callback setters
    void set_message_callback(BinancePublicMessageCallback callback);
    void set_orderbook_callback(BinanceOrderbookCallback callback);
    void set_ticker_callback(BinanceTickerCallback callback);
    void set_trade_callback(BinanceTradeCallback callback);
    
    // Lifecycle
    bool initialize() override;
    void shutdown() override;
    
private:
    void message_loop();
    void handle_public_message(const std::string& message);
    void handle_orderbook_update(const std::string& symbol, const std::string& data);
    void handle_ticker_update(const std::string& symbol, const std::string& data);
    void handle_trade_update(const std::string& symbol, const std::string& data);
    void handle_kline_update(const std::string& symbol, const std::string& data);
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread message_thread_;
    std::mutex callback_mutex_;
    
    // Callbacks
    BinancePublicMessageCallback message_callback_;
    BinanceOrderbookCallback orderbook_callback_;
    BinanceTickerCallback ticker_callback_;
    BinanceTradeCallback trade_callback_;
    
    // WebSocket connection
    std::string websocket_url_;
    std::vector<std::string> subscribed_channels_;
    std::mutex channels_mutex_;
};

} // namespace binance
