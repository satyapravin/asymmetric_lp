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

namespace deribit {

// Deribit Private WebSocket message types (User Data)
enum class DeribitPrivateMessageType {
    ORDER_UPDATE,
    ACCOUNT_UPDATE,
    BALANCE_UPDATE,
    POSITION_UPDATE,
    TRADE_UPDATE,
    PORTFOLIO_UPDATE,
    ERROR_MESSAGE
};

// Deribit Private WebSocket message structure
struct DeribitPrivateWebSocketMessage {
    DeribitPrivateMessageType type;
    std::string data;
    std::string symbol;
    std::string order_id;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// Deribit Private WebSocket callback types
using DeribitPrivateMessageCallback = std::function<void(const DeribitPrivateWebSocketMessage& message)>;
using DeribitPrivateOrderCallback = std::function<void(const std::string& order_id, const std::string& status)>;

// Deribit Private WebSocket Handler (User Data)
class DeribitPrivateWebSocketHandler : public IExchangeWebSocketHandler {
public:
    DeribitPrivateWebSocketHandler(const std::string& client_id, const std::string& client_secret);
    ~DeribitPrivateWebSocketHandler();

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
    bool subscribe_to_channel(const std::string& channel) override; // For user data stream
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    WebSocketType get_type() const override { return WebSocketType::PRIVATE_USER_DATA; }
    std::string get_exchange_name() const override { return "DERIBIT"; }
    std::string get_channel() const override { return "private"; }

    // Deribit-specific private subscriptions
    bool subscribe_to_user_data();
    bool subscribe_to_order_updates(const std::string& symbol = "");
    bool subscribe_to_account_updates();
    bool subscribe_to_portfolio_updates();
    bool subscribe_to_position_updates();

    // Authentication specific
    std::string get_client_id() const { return client_id_; }
    std::string get_client_secret() const { return client_secret_; }
    std::string get_access_token() const { return access_token_; }
    bool authenticate(); // Authenticate and get access token
    bool refresh_token(); // Refresh access token

    // Message handling
    void handle_message(const std::string& message);

private:
    std::string client_id_;
    std::string client_secret_;
    std::string access_token_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<bool> should_stop_{false};
    std::thread token_refresh_thread_;
    std::atomic<bool> refresh_thread_running_{false};
    
    // Deribit-specific
    std::atomic<uint32_t> request_id_{1};
    std::string api_version_{"2.0"};
    std::string testnet_url_{"wss://test.deribit.com/ws/api/v2"};
    std::string mainnet_url_{"wss://www.deribit.com/ws/api/v2"};
    bool use_testnet_{true};
    
    void token_refresh_loop();
    std::string build_auth_message();
    std::string build_subscription_message(const std::string& method, const std::vector<std::string>& channels);
    void process_order_update(const std::string& message);
    void process_account_update(const std::string& message);
    void process_portfolio_update(const std::string& message);
    void process_position_update(const std::string& message);
};

} // namespace deribit
