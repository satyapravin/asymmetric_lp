#pragma once
#include "../../i_exchange_pms.hpp"
#include "../../../proto/position.pb.h"
#include "../../../proto/acc_balance.pb.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <json/json.h>

namespace binance {

struct BinancePMSConfig {
    std::string api_key;
    std::string api_secret;
    std::string websocket_url;
    bool testnet{false};
    std::string asset_type{"futures"};
    int timeout_ms{30000};
    int max_retries{3};
};

class BinancePMS : public IExchangePMS {
public:
    BinancePMS(const BinancePMSConfig& config);
    ~BinancePMS();
    
    // Connection management
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Real-time callbacks only (no query methods)
    void set_position_update_callback(PositionUpdateCallback callback) override;
    void set_account_balance_update_callback(AccountBalanceUpdateCallback callback) override;
    
    // Testing interface
    void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) override;

private:
    BinancePMSConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    void* websocket_handle_{nullptr};
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Custom transport for testing
    std::shared_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Callbacks
    PositionUpdateCallback position_update_callback_;
    AccountBalanceUpdateCallback account_balance_update_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_websocket_message(const std::string& message);
    void handle_position_update(const Json::Value& position_data);
    void handle_account_update(const Json::Value& account_data);
    void handle_balance_update(const Json::Value& balance_data);
    
    // Authentication
    bool authenticate_websocket();
    std::string create_auth_message();
    
    // Utility methods
    std::string generate_request_id();
};

} // namespace binance
