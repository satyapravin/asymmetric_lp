#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <json/json.h>
#include "../../utils/oms/exchange_oms.hpp"
#include "../../utils/oms/order_state.hpp"
#include "../../utils/oms/order.hpp"
#include "binance_data_fetcher.hpp"
#include "binance_websocket_handlers.hpp"
#include "../config/api_endpoint_config.hpp"
#include "../../utils/handlers/http/i_http_handler.hpp"
#include <vector>
#include <cstdint>

namespace binance {

struct BinanceConfig {
    std::string api_key;
    std::string api_secret;
    std::string exchange_name = "BINANCE";
    exchange_config::AssetType asset_type = exchange_config::AssetType::FUTURES;  // Default to futures
    std::string config_file = "";  // Optional custom config file
    int max_retries = 3;
    int timeout_ms = 5000;
    double fill_probability = 0.8;
    double reject_probability = 0.1;
};

class BinanceOMS : public IExchangeOMS {
public:
    explicit BinanceOMS(const BinanceConfig& config);
    ~BinanceOMS();

    // IExchangeOMS interface
    Result<bool> connect() override;
    void disconnect() override;
    bool is_connected() const override;
    std::string get_exchange_name() const override;
    
    // Order operations (async - responses come via WebSocket)
    Result<std::string> send_order(const Order& order) override;
    Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override;
    Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id, 
                     double new_price, double new_qty) override;
    
    // Exchange information
    std::vector<std::string> get_supported_symbols() const override;
    
    // Health and monitoring
    Result<std::map<std::string, std::string>> get_health_status() const override;
    Result<std::map<std::string, double>> get_performance_metrics() const override;

    // Exchange data access
    std::vector<BinanceOrder> get_active_orders();
    std::vector<BinanceOrder> get_order_history(const std::string& symbol = "", 
                                               uint64_t start_time = 0, 
                                               uint64_t end_time = 0);
    std::vector<BinancePosition> get_positions();
    std::vector<BinanceTrade> get_trade_history(const std::string& symbol = "",
                                               uint64_t start_time = 0,
                                               uint64_t end_time = 0);
    std::vector<BinanceBalance> get_balances();

    // Configuration management
    void set_asset_type(exchange_config::AssetType asset_type);
    exchange_config::AssetType get_asset_type() const;
    exchange_config::AssetConfig get_asset_config() const;

private:
    // HTTP client methods using handlers
    HttpResponse make_request(const std::string& endpoint, const std::string& method = "GET", 
                              const std::string& body = "", bool is_signed = false);
    std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body);
    std::string generate_signature(const std::string& data);
    
    // WebSocket methods using handlers
    void setup_websocket_callbacks();
    void handle_websocket_message(const std::string& message);
    
    // Order processing
    void process_order_update(const Json::Value& data);
    void process_account_update(const Json::Value& data);
    std::string create_order_payload(const Order& order);
    
    // Rate limiting
    bool check_rate_limit();
    void update_rate_limit();
    
    // Configuration
    exchange_config::AssetType current_asset_type_;
    exchange_config::AssetConfig asset_config_;
    
    // API endpoint management
    std::string get_endpoint_url(const std::string& endpoint_name) const;
    exchange_config::EndpointConfig get_endpoint_config(const std::string& endpoint_name) const;
    
    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    // Handlers (dependency injection)
    std::unique_ptr<IHttpHandler> http_handler_;
    // WebSocket management
    std::shared_ptr<BinanceWebSocketManager> ws_manager_;
    
    // Event callbacks
    std::function<void(const std::string& order_id, const std::string& status)> order_event_callback_;
    std::function<void(const std::string& trade_id, double qty, double price)> trade_event_callback_;
    
    // WebSocket message handlers
    void handle_order_update(const std::string& order_id, const std::string& status);
    void handle_trade_update(const std::string& trade_id, double qty, double price);
    
    // WebSocket state
    std::string listen_key_;
    std::atomic<bool> ws_connected_{false};
    
    // Order tracking
    std::map<std::string, OrderStateInfo> orders_;
    std::mutex orders_mutex_;
    
    // Rate limiting
    std::atomic<int> requests_per_minute_{0};
    std::chrono::steady_clock::time_point last_reset_;
    std::mutex rate_limit_mutex_;
    
    // Data fetcher
    std::unique_ptr<BinanceDataFetcher> data_fetcher_;
};

} // namespace binance
