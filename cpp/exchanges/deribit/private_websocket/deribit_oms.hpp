#pragma once
#include "../../i_exchange_oms.hpp"
#include "../../../proto/order.pb.h"
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

struct DeribitOMSConfig {
    std::string client_id;
    std::string client_secret;
    std::string access_token;
    std::string refresh_token;
    std::string base_url;
    std::string websocket_url;
    bool testnet{true};
    std::string currency{"BTC"};
    int timeout_ms{30000};
    int max_retries{3};
};

class DeribitOMS : public IExchangeOMS {
public:
    DeribitOMS(const DeribitOMSConfig& config);
    ~DeribitOMS();
    
    // Connection management
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Order management (via WebSocket)
    bool cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) override;
    bool replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) override;
    proto::OrderEvent get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) override;
    
    // Specific order types (via WebSocket)
    bool place_market_order(const std::string& symbol, const std::string& side, double quantity) override;
    bool place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) override;
    
    // Real-time callbacks
    void set_order_status_callback(OrderStatusCallback callback) override;
    
    // WebSocket transport injection for testing
    void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) override;
    
    // Testing helpers (exposed for integration tests)
    void handle_websocket_message(const std::string& message);  // Made public for testing
    std::string create_order_message(const std::string& symbol, const std::string& side, 
                                   double quantity, double price, const std::string& order_type);  // Made public for testing
    std::string create_cancel_message(const std::string& cl_ord_id, const std::string& exch_ord_id);  // Made public for testing

private:
    DeribitOMSConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Custom WebSocket transport for testing
    std::shared_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Callbacks
    OrderStatusCallback order_status_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_order_update(const Json::Value& order_data);
    void handle_trade_update(const Json::Value& trade_data);
    
    // Order management
    std::string create_replace_message(const std::string& cl_ord_id, const proto::OrderRequest& new_order);
    
    // Authentication
    bool authenticate_websocket();
    std::string create_auth_message();
    std::string get_access_token();
    
    // Utility methods
    std::string generate_request_id();
    proto::OrderEventType map_order_status(const std::string& status);
    std::string map_side_to_deribit(const std::string& side);
    std::string map_order_type_to_deribit(const std::string& order_type);
};

} // namespace deribit