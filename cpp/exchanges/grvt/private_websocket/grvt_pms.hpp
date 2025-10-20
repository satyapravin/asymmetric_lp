#pragma once
#include "../../i_exchange_pms.hpp"
#include "../../../proto/position.pb.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <json/json.h>

namespace grvt {

struct GrvtPMSConfig {
    std::string api_key;
    std::string session_cookie;
    std::string account_id;
    std::string sub_account_id;  // Added for sub-account polling
    std::string websocket_url;
    std::string rest_api_url;    // Added for REST API polling
    bool testnet{false};
    bool use_lite_version{false};
    int timeout_ms{30000};
    int max_retries{3};
    int polling_interval_seconds{10};  // Added for configurable polling interval
};

class GrvtPMS : public IExchangePMS {
public:
    GrvtPMS(const GrvtPMSConfig& config);
    ~GrvtPMS();
    
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

    // GRVT-specific configuration
    void set_sub_account_id(const std::string& sub_account_id) { config_.sub_account_id = sub_account_id; }
    void set_polling_interval(int interval_seconds) { config_.polling_interval_seconds = interval_seconds; }

private:
    GrvtPMSConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    void* websocket_handle_{nullptr};
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Balance polling thread
    std::thread polling_thread_;
    std::atomic<bool> polling_running_{false};
    
    // Callbacks
    PositionUpdateCallback position_update_callback_;
    AccountBalanceUpdateCallback account_balance_update_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_websocket_message(const std::string& message);
    void handle_position_update(const Json::Value& position_data);
    void handle_account_update(const Json::Value& account_data);
    void handle_balance_update(const Json::Value& balance_data);
    
    // Balance polling methods
    void polling_loop();
    void poll_account_balances();
    std::string create_balance_request();
    bool parse_balance_response(const std::string& response);
    
    // Authentication
    bool authenticate_websocket();
    std::string create_auth_message();
    
    // Utility methods
    std::string generate_request_id();
};

} // namespace grvt
