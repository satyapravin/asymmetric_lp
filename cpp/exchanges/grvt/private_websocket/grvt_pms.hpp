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
    std::string websocket_url;
    bool testnet{false};
    bool use_lite_version{false};
    int timeout_ms{30000};
    int max_retries{3};
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

private:
    GrvtPMSConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<uint32_t> request_id_{1};
    
    // WebSocket connection
    void* websocket_handle_{nullptr};
    std::thread websocket_thread_;
    std::atomic<bool> websocket_running_{false};
    
    // Callbacks
    PositionUpdateCallback position_update_callback_;
    
    // Message handling
    void websocket_loop();
    void handle_websocket_message(const std::string& message);
    void handle_position_update(const Json::Value& position_data);
    void handle_account_update(const Json::Value& account_data);
    
    // Authentication
    bool authenticate_websocket();
    std::string create_auth_message();
    
    // Utility methods
    std::string generate_request_id();
};

} // namespace grvt
