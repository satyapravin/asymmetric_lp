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

namespace binance {

// Binance Private WebSocket message types (User Data)
enum class BinancePrivateMessageType {
    ORDER_UPDATE,
    ACCOUNT_UPDATE,
    BALANCE_UPDATE,
    POSITION_UPDATE,
    TRADE_UPDATE,
    ERROR_MESSAGE
};

// Binance Private WebSocket message structure
struct BinancePrivateWebSocketMessage {
    BinancePrivateMessageType type;
    std::string data;
    std::string symbol;
    std::string order_id;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// Binance Private WebSocket callback types
using BinancePrivateMessageCallback = std::function<void(const BinancePrivateWebSocketMessage& message)>;
using BinanceOrderCallback = std::function<void(const std::string& order_id, const std::string& status)>;

// Binance Private WebSocket Handler (User Data)
class BinancePrivateWebSocketHandler : public IExchangeWebSocketHandler {
public:
    BinancePrivateWebSocketHandler(const std::string& api_key, const std::string& api_secret);
    ~BinancePrivateWebSocketHandler();

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
    bool subscribe_to_channel(const std::string& channel) override; // For user data stream, channel is typically the listenKey
    bool unsubscribe_from_channel(const std::string& channel) override;
    std::vector<std::string> get_subscribed_channels() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    WebSocketType get_type() const override { return WebSocketType::PRIVATE_USER_DATA; }
    std::string get_exchange_name() const override { return "BINANCE"; }
    std::string get_channel() const override { return "private"; }

    // Binance-specific private subscriptions
    bool subscribe_to_user_data();
    bool subscribe_to_order_updates();
    bool subscribe_to_account_updates();
    bool subscribe_to_balance_updates();
    bool subscribe_to_position_updates();

    // Authentication specific
    std::string get_listen_key() const { return listen_key_; }
    bool refresh_listen_key(); // Simulate listen key refresh

    // Message handling
    void handle_message(const std::string& message);

private:
    std::string api_key_;
    std::string api_secret_;
    std::string listen_key_;
    std::atomic<bool> connected_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<bool> should_stop_{false};
    std::thread listen_key_refresh_thread_;
    std::atomic<bool> refresh_thread_running_{false};
    
    // WebSocket connection management
    std::string websocket_url_;
    std::atomic<int> ping_interval_{30};
    std::atomic<int> timeout_{30};
    std::atomic<int> reconnect_attempts_{5};
    std::atomic<int> reconnect_delay_{5};
    
    // Connection thread
    std::thread connection_thread_;
    std::atomic<bool> connection_thread_running_{false};
    
    void connection_loop();
    void listen_key_refresh_loop();
    void handle_websocket_message(const std::string& message);
    std::string create_listen_key();
    bool keep_alive_listen_key();
};

} // namespace binance