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

namespace grvt {

struct GrvtSubscriberConfig {
    std::string websocket_url;
    bool testnet{false};
    bool use_lite_version{false};  // Use 'lite' variant with shortened field names
    bool use_snapshot_channels{true};  // Use snapshot (.s) vs delta (.d) channels
    int timeout_ms{30000};
    int max_retries{3};
};

class GrvtSubscriber : public IExchangeSubscriber {
public:
    GrvtSubscriber(const GrvtSubscriberConfig& config);
    ~GrvtSubscriber();
    
    // Connection management
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void start() override;
    void stop() override;
    
    // Market data subscriptions (via WebSocket)
    bool subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) override;
    bool subscribe_trades(const std::string& symbol) override;
    bool subscribe_ticker(const std::string& symbol);  // Additional GRVT-specific method
    bool unsubscribe(const std::string& symbol) override;
    
    // Real-time callbacks
    void set_orderbook_callback(OrderbookCallback callback) override;
    void set_trade_callback(TradeCallback callback) override;
    void set_error_callback(std::function<void(const std::string&)> callback) override;
    
    // Testing interface - inject custom WebSocket transport
    void set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) override;
    
    // Testing helpers (exposed for integration tests)
    void handle_websocket_message(const std::string& message);  // Made public for testing
    std::string create_subscription_message(const std::string& symbol, const std::string& channel, bool use_snapshot = true);  // Made public for testing
    std::string get_channel_name(const std::string& base_channel, bool use_snapshot) const;  // Made public for testing

private:
    GrvtSubscriberConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    void* websocket_handle_{nullptr};
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Subscribed symbols
    std::vector<std::string> subscribed_symbols_;
    std::mutex symbols_mutex_;
    
    // Custom WebSocket transport for testing
    std::unique_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Callbacks
    OrderbookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_orderbook_update(const Json::Value& orderbook_data);
    void handle_trade_update(const Json::Value& trade_data);
    
    // Subscription management (private)
    std::string create_unsubscription_message(const std::string& symbol, const std::string& channel, bool use_snapshot = true);
    
    // Utility methods
    std::string generate_request_id();
};

} // namespace grvt
