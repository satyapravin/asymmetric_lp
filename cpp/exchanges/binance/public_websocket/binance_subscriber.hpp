#pragma once
#include "../../i_exchange_subscriber.hpp"
#include "../../../proto/market_data.pb.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <json/json.h>

namespace binance {

struct BinanceSubscriberConfig {
    std::string websocket_url;
    bool testnet{false};
    std::string asset_type{"futures"};
    int timeout_ms{30000};
    int max_retries{3};
};

class BinanceSubscriber : public IExchangeSubscriber {
public:
    BinanceSubscriber(const BinanceSubscriberConfig& config);
    ~BinanceSubscriber();
    
    // Connection management
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void start() override;
    void stop() override;
    
    // Market data subscriptions (via WebSocket)
    bool subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) override;
    bool subscribe_trades(const std::string& symbol) override;
    bool unsubscribe(const std::string& symbol) override;
    
    // Real-time callbacks
    void set_orderbook_callback(OrderbookCallback callback) override;
    void set_trade_callback(TradeCallback callback) override;
    void set_error_callback(std::function<void(const std::string&)> callback) override;
    
    // Testing interface - inject custom WebSocket transport
    void set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) override;

private:
    BinanceSubscriberConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    void* websocket_handle_{nullptr};
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Custom WebSocket transport for testing
    std::unique_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Callbacks
    OrderbookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // Subscribed symbols
    std::vector<std::string> subscribed_symbols_;
    std::mutex symbols_mutex_;
    
    // Message handling
    void websocket_loop();
    void handle_websocket_message(const std::string& message);
    void handle_orderbook_update(const Json::Value& orderbook_data);
    void handle_trade_update(const Json::Value& trade_data);
    
    // Subscription management
    std::string create_subscription_message(const std::string& symbol, const std::string& channel);
    std::string create_unsubscription_message(const std::string& symbol, const std::string& channel);
    
    // Utility methods
    std::string generate_request_id();
    std::string convert_symbol_to_binance(const std::string& symbol);
};

} // namespace binance
