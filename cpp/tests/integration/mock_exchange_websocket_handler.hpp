#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <json/json.h>
#include "../exchanges/websocket/i_exchange_websocket_handler.hpp"
#include "../../proto/order.pb.h"
#include "../../proto/market_data.pb.h"
#include "../../proto/position.pb.h"
#include "../../proto/acc_balance.pb.h"

namespace testing {

/**
 * Mock Exchange WebSocket Handler
 * 
 * This handler simulates real exchange WebSocket behavior by:
 * - Reading JSON message files from disk
 * - Parsing them into protobuf messages
 * - Calling the appropriate callbacks
 * - Simulating realistic timing and message flow
 * 
 * This tests the complete pipeline up to the transport layer without mocking exchange logic.
 */
class MockExchangeWebSocketHandler : public IExchangeWebSocketHandler {
public:
    MockExchangeWebSocketHandler(const std::string& exchange_name, const std::string& data_path);
    ~MockExchangeWebSocketHandler();

    // IExchangeWebSocketHandler interface
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;

    // Order management callbacks
    void set_order_event_callback(OrderEventCallback callback) override;
    void set_trade_execution_callback(TradeExecutionCallback callback) override;

    // Market data callbacks
    void set_orderbook_callback(OrderBookCallback callback) override;
    void set_trade_callback(TradeCallback callback) override;

    // Position/balance callbacks
    void set_position_update_callback(PositionUpdateCallback callback) override;
    void set_account_balance_update_callback(AccountBalanceUpdateCallback callback) override;

    // Order management
    bool send_order(const std::string& cl_ord_id, const std::string& symbol, 
                   proto::Side side, proto::OrderType type, double qty, double price) override;
    bool cancel_order(const std::string& cl_ord_id) override;
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty) override;

    // Mock-specific methods
    void load_test_data();
    void start_message_simulation();
    void stop_message_simulation();
    void simulate_order_response(const std::string& cl_ord_id, const std::string& response_type);
    void simulate_market_data_stream();
    void simulate_position_updates();
    void simulate_balance_updates();

    // Configuration
    void set_message_delay_ms(int delay) { message_delay_ms_ = delay; }
    void set_simulation_mode(bool enabled) { simulation_mode_ = enabled; }

private:
    std::string exchange_name_;
    std::string data_path_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<bool> simulation_running_{false};
    bool simulation_mode_{true};
    int message_delay_ms_{100};

    // Callbacks
    OrderEventCallback order_event_callback_;
    TradeExecutionCallback trade_execution_callback_;
    OrderBookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    PositionUpdateCallback position_update_callback_;
    AccountBalanceUpdateCallback balance_update_callback_;

    // Message simulation threads
    std::thread market_data_thread_;
    std::thread order_response_thread_;
    std::thread position_update_thread_;
    std::thread balance_update_thread_;

    // Test data storage
    std::vector<std::string> orderbook_messages_;
    std::vector<std::string> trade_messages_;
    std::vector<std::string> order_update_templates_;
    std::string position_update_template_;
    std::string balance_update_template_;

    // Order tracking for realistic responses
    std::map<std::string, OrderInfo> pending_orders_;
    std::mutex orders_mutex_;
    std::atomic<uint64_t> order_counter_{0};

    // Internal methods
    void market_data_simulation_loop();
    void order_response_simulation_loop();
    void position_update_simulation_loop();
    void balance_update_simulation_loop();
    
    std::string load_json_file(const std::string& filename);
    bool parse_orderbook_message(const std::string& json_data, proto::OrderBookSnapshot& snapshot);
    bool parse_trade_message(const std::string& json_data, proto::Trade& trade);
    bool parse_order_update(const std::string& json_data, proto::OrderEvent& order_event);
    bool parse_position_update(const std::string& json_data, proto::PositionUpdate& position);
    bool parse_balance_update(const std::string& json_data, proto::AccountBalanceUpdate& balance);
    
    std::string replace_order_placeholders(const std::string& template_json, 
                                          const std::string& cl_ord_id,
                                          const std::string& symbol,
                                          proto::Side side,
                                          proto::OrderType type,
                                          double qty,
                                          double price);
    
    void simulate_realistic_order_response(const std::string& cl_ord_id);
    std::string generate_exchange_order_id();

    // Order tracking structure
    struct OrderInfo {
        std::string cl_ord_id;
        std::string symbol;
        proto::Side side;
        proto::OrderType type;
        double qty;
        double price;
        std::chrono::system_clock::time_point timestamp;
        proto::OrderStatus status;
    };
};

} // namespace testing
