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

// Deribit WebSocket message types
enum class DeribitMessageType {
    ORDERBOOK_UPDATE,
    TRADE_UPDATE,
    TICKER_UPDATE,
    INSTRUMENT_UPDATE,
    MARKET_DATA,
    ERROR_MESSAGE
};

// Deribit WebSocket message structure
struct DeribitWebSocketMessage {
    DeribitMessageType type;
    std::string data;
    std::string symbol;
    std::string channel;
    uint64_t timestamp_us;
    bool is_binary{false};
};

// Deribit WebSocket callback types
using DeribitMessageCallback = std::function<void(const DeribitWebSocketMessage& message)>;
using DeribitOrderbookCallback = std::function<void(const std::string& symbol, const std::string& data)>;
using DeribitTradeCallback = std::function<void(const std::string& symbol, const std::string& data)>;

// Deribit Public WebSocket Handler (Market Data)
class DeribitPublicWebSocketHandler : public IExchangeWebSocketHandler {
public:
    DeribitPublicWebSocketHandler();
    ~DeribitPublicWebSocketHandler();

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
    bool is_authenticated() const override { return true; } // Public streams don't require explicit auth
    WebSocketType get_type() const override { return WebSocketType::PUBLIC_MARKET_DATA; }
    std::string get_exchange_name() const override { return "DERIBIT"; }
    std::string get_channel() const override { return "public"; }

    // Deribit-specific public subscriptions
    bool subscribe_to_orderbook(const std::string& symbol, const std::string& interval = "100ms");
    bool subscribe_to_trades(const std::string& symbol);
    bool subscribe_to_ticker(const std::string& symbol);
    bool subscribe_to_instruments(const std::string& currency = "BTC");

    // Message handling
    void handle_message(const std::string& message);

private:
    std::atomic<bool> connected_{false};
    std::atomic<WebSocketState> state_{WebSocketState::DISCONNECTED};
    WebSocketMessageCallback message_callback_;
    WebSocketErrorCallback error_callback_;
    WebSocketConnectCallback connect_callback_;
    std::vector<std::string> subscribed_channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<bool> should_stop_{false};
    
    // Deribit-specific
    std::atomic<uint32_t> request_id_{1};
    std::string api_version_{"2.0"};
    
    // Helper methods
    std::string build_subscription_message(const std::string& method, const std::vector<std::string>& channels);
    void process_orderbook_update(const std::string& message);
    void process_trade_update(const std::string& message);
    void process_ticker_update(const std::string& message);
    void process_instrument_update(const std::string& message);
};

} // namespace deribit
