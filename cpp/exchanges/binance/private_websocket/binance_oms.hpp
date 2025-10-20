#pragma once
#include "../../i_exchange_oms.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <cstdint>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace binance {

// Binance OMS Configuration
struct BinanceConfig {
    std::string api_key;
    std::string api_secret;
    std::string base_url;
    bool testnet{false};
    int max_retries{3};
    int timeout_ms{5000};
};

// Binance Order Management System
class BinanceOMS : public IExchangeOMS {
public:
    BinanceOMS(const BinanceConfig& config);
    ~BinanceOMS();

    // IExchangeOMS interface
    // Connection management
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;

    // Order management
    bool cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) override;
    bool replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) override;
    proto::OrderEvent get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) override;
    
    // Specific order types
    bool place_market_order(const std::string& symbol, const std::string& side, double quantity) override;
    bool place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) override;

    // Real-time callbacks
    void set_order_status_callback(OrderStatusCallback callback) override;
    
    // WebSocket transport injection for testing
    void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) override;

private:
    BinanceConfig config_;
    std::atomic<bool> connected_;
    std::atomic<bool> authenticated_;
    OrderStatusCallback order_callback_;
    std::shared_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // HTTP client for API calls
    std::string make_request(const std::string& endpoint, const std::string& method = "GET", 
                            const std::string& body = "", bool is_signed = false);
    
    // Authentication helpers
    std::string generate_signature(const std::string& data);
    std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body);
    
    // Order conversion helpers
    std::string order_side_to_string(const std::string& side);
    std::string order_type_to_string(const std::string& type);
    
    // JSON parsing helpers
    proto::OrderEvent parse_order_from_json(const std::string& json_str);
    
    // Error handling
    std::string get_error_message(const std::string& response);
};

} // namespace binance