#pragma once
#include "../../websocket/i_exchange_websocket_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <cstdint>

namespace grvt {

// GRVT Private WebSocket message types (User Data)
enum class GrvtPrivateMessageType {
    ORDER_UPDATE,
    ACCOUNT_UPDATE,
    BALANCE_UPDATE,
    POSITION_UPDATE,
    TRADE_UPDATE,
    ERROR_MESSAGE
};

// GRVT Private WebSocket message structure
struct GrvtPrivateWebSocketMessage {
    GrvtPrivateMessageType type;
    std::string data;
    std::string symbol;
    std::string order_id;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// GRVT Private WebSocket callback types
using GrvtPrivateMessageCallback = std::function<void(const GrvtPrivateWebSocketMessage& message)>;
using GrvtPrivateOrderCallback = std::function<void(const std::string& order_id, const std::string& status)>;
using GrvtAccountUpdateCallback = std::function<void(const std::string& account_id, const std::string& data)>;

// GRVT Private WebSocket Handler (User Data)
class GrvtPrivateWebSocketHandler : public IExchangeWebSocketHandler {
public:
    GrvtPrivateWebSocketHandler(const std::string& api_key, const std::string& session_cookie, const std::string& account_id);
    ~GrvtPrivateWebSocketHandler();

    // IWebSocketHandler interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    WebSocketState get_state() const override;
    bool send_message(const std::string& message, bool binary = false) override;
    bool send_binary(const std::vector<uint8_t>& data) override;
    void set_message_callback(WebSocketMessageCallback callback) override;
    void set_error_callback(WebSocketErrorCallback callback) override;
    void set_connect_callback(WebSocketConnectCallback callback) override;
    void set_ping_interval(int seconds) override;
    void set_timeout(int seconds) override;
    void set_reconnect_attempts(int attempts) override;
    void set_reconnect_delay(int seconds) override;
    bool initialize() override;
    void shutdown() override;
    bool subscribe_to_channel(const std::string& channel) override;
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    std::string get_channel() const override;
    bool is_authenticated() const override;
    WebSocketType get_type() const override { return WebSocketType::PRIVATE_USER_DATA; }
    std::string get_exchange_name() const override { return "GRVT"; }

    // GRVT-specific private subscriptions
    bool subscribe_to_user_data();
    bool subscribe_to_order_updates();
    bool subscribe_to_account_updates();
    bool subscribe_to_balance_updates();
    bool subscribe_to_position_updates();

    // Authentication specific
    bool authenticate();
    bool refresh_session();
    bool validate_session();

    // JSON-RPC methods
    bool send_jsonrpc_request(const std::string& method, const std::string& params, int request_id);
    bool send_lite_jsonrpc_request(const std::string& method, const std::string& params, int request_id);

    // Message handling
    void handle_message(const std::string& message);
    void handle_order_update(const std::string& order_id, const std::string& status);
    void handle_account_update(const std::string& account_id, const std::string& data);

private:
    std::string api_key_;
    std::string session_cookie_;
    std::string account_id_;
    std::atomic<bool> connected_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<bool> should_stop_{false};
    std::thread session_refresh_thread_;
    std::atomic<bool> refresh_thread_running_{false};

    // GRVT-specific callbacks
    GrvtPrivateOrderCallback order_callback_;
    GrvtAccountUpdateCallback account_callback_;

    // Connection management
    std::string base_url_;
    bool use_lite_version_{false};
    std::atomic<int> request_counter_{0};

    // Session management
    void session_refresh_loop();
    bool parse_jsonrpc_response(const std::string& message);
    std::string create_jsonrpc_request(const std::string& method, const std::string& params, int request_id);
    std::string create_lite_jsonrpc_request(const std::string& method, const std::string& params, int request_id);
};

} // namespace grvt
