#pragma once
#include "i_exchange_websocket_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

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
using BinanceAccountCallback = std::function<void(const std::string& account_data)>;
using BinanceBalanceCallback = std::function<void(const std::string& asset, double balance)>;

// Binance Private WebSocket Handler (User Data)
class BinancePrivateWebSocketHandler : public IExchangeWebSocketHandler {
public:
    BinancePrivateWebSocketHandler(const std::string& api_key, const std::string& api_secret);
    ~BinancePrivateWebSocketHandler();
    
    // IWebSocketHandler interface
    bool connect(const std::string& url) override;
    void disconnect() override;
    bool is_connected() const override;
    void send_message(const std::string& message) override;
    void send_binary(const std::vector<uint8_t>& data) override;
    
    // IExchangeWebSocketHandler interface
    WebSocketType get_type() const override { return WebSocketType::PRIVATE_USER_DATA; }
    std::string get_exchange_name() const override { return "BINANCE"; }
    
    // Private WebSocket specific methods
    bool subscribe_to_user_data();
    bool subscribe_to_order_updates();
    bool subscribe_to_account_updates();
    bool unsubscribe_from_channel(const std::string& channel);
    
    // Callback setters
    void set_message_callback(BinancePrivateMessageCallback callback);
    void set_order_callback(BinanceOrderCallback callback);
    void set_account_callback(BinanceAccountCallback callback);
    void set_balance_callback(BinanceBalanceCallback callback);
    
    // Lifecycle
    bool initialize() override;
    void shutdown() override;
    
private:
    void message_loop();
    void handle_private_message(const std::string& message);
    void handle_user_data_message(const std::string& data);
    void handle_account_update(const std::string& data);
    void handle_order_update(const std::string& data);
    void handle_balance_update(const std::string& data);
    
    std::string generate_listen_key();
    void refresh_listen_key();
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread message_thread_;
    std::thread refresh_thread_;
    std::mutex callback_mutex_;
    
    // API credentials
    std::string api_key_;
    std::string api_secret_;
    std::string listen_key_;
    
    // Callbacks
    BinancePrivateMessageCallback message_callback_;
    BinanceOrderCallback order_callback_;
    BinanceAccountCallback account_callback_;
    BinanceBalanceCallback balance_callback_;
    
    // WebSocket connection
    std::string websocket_url_;
    std::vector<std::string> subscribed_channels_;
    std::mutex channels_mutex_;
};

} // namespace binance
