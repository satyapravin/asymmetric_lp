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

// Forward declaration
namespace websocket_transport {
    class IWebSocketTransport;
    struct WebSocketMessage;
}

namespace deribit {

struct DeribitSubscriberConfig {
    std::string websocket_url;
    bool testnet{true};
    std::string currency{"BTC"};
    int timeout_ms{30000};
    int max_retries{3};
};

class DeribitSubscriber : public IExchangeSubscriber {
public:
    DeribitSubscriber(const DeribitSubscriberConfig& config);
    ~DeribitSubscriber();
    
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
    
    // Testing helpers (exposed for integration tests)
    void handle_websocket_message(const std::string& message);  // Made public for testing
    std::string create_subscription_message(const std::string& symbol, const std::string& channel, const std::string& interval = "raw");  // Made public for testing
    std::string create_unsubscription_message(const std::string& symbol, const std::string& channel, const std::string& interval = "raw");  // Made public for testing

private:
    DeribitSubscriberConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Custom WebSocket transport for testing
    std::unique_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Subscribed symbols
    std::vector<std::string> subscribed_symbols_;
    std::mutex symbols_mutex_;
    
    // Callbacks
    OrderbookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_orderbook_update(const Json::Value& orderbook_data, const std::string& symbol);
    void handle_trade_update(const Json::Value& trade_data, const std::string& symbol);
    
    // Utility methods
    std::string generate_request_id();
    std::string get_interval_string(int frequency_ms);
};

} // namespace deribit
