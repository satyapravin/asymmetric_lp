#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "../../exchanges/i_exchange_oms.hpp"
#include "../../exchanges/i_exchange_subscriber.hpp"
#include "../../exchanges/i_exchange_pms.hpp"
#include "../../exchanges/i_exchange_data_fetcher.hpp"
#include "../../proto/order.pb.h"
#include "../../proto/market_data.pb.h"
#include "../../proto/position.pb.h"
#include "../../proto/acc_balance.pb.h"
#include <json/json.h>

namespace testing {

/**
 * Mock Exchange Implementation
 * 
 * This mock exchange simulates real exchange behavior using JSON data files.
 * It handles:
 * - Order management (ACK, FILL, REJECT, CANCEL)
 * - Market data streaming (orderbook, trades)
 * - Position and balance updates
 * - HTTP API responses
 */
class MockExchange : public IExchangeOMS, 
                     public IExchangeSubscriber, 
                     public IExchangePMS,
                     public IExchangeDataFetcher {
public:
    MockExchange(const std::string& exchange_name, const std::string& data_path);
    ~MockExchange();

    // IExchangeOMS interface
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    void set_order_event_callback(OrderEventCallback callback) override;
    void set_trade_execution_callback(TradeExecutionCallback callback) override;
    bool send_order(const std::string& cl_ord_id, const std::string& symbol, 
                   proto::Side side, proto::OrderType type, double qty, double price) override;
    bool cancel_order(const std::string& cl_ord_id) override;
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty) override;

    // IExchangeSubscriber interface
    void set_orderbook_callback(OrderBookCallback callback) override;
    void set_trade_callback(TradeCallback callback) override;
    bool subscribe_to_orderbook(const std::string& symbol) override;
    bool subscribe_to_trades(const std::string& symbol) override;

    // IExchangePMS interface
    void set_position_update_callback(PositionUpdateCallback callback) override;
    void set_account_balance_update_callback(AccountBalanceUpdateCallback callback) override;

    // IExchangeDataFetcher interface
    std::vector<proto::OrderBookSnapshot> get_orderbook_snapshots(
        const std::string& symbol, int limit = 100) override;
    std::vector<proto::Trade> get_recent_trades(
        const std::string& symbol, int limit = 100) override;
    std::vector<proto::OrderEvent> get_open_orders(
        const std::string& symbol = "") override;
    std::vector<proto::AccountBalance> get_balances() override;

    // Mock-specific methods
    void start_market_data_simulation();
    void stop_market_data_simulation();
    void simulate_order_response(const std::string& cl_ord_id, const std::string& response_type);
    void load_test_data();

private:
    std::string exchange_name_;
    std::string data_path_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<bool> running_{false};

    // Callbacks
    OrderEventCallback order_event_callback_;
    TradeExecutionCallback trade_execution_callback_;
    OrderBookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    PositionUpdateCallback position_update_callback_;
    AccountBalanceUpdateCallback balance_update_callback_;

    // Simulation threads
    std::thread market_data_thread_;
    std::thread order_response_thread_;
    std::atomic<bool> market_data_running_{false};
    std::atomic<bool> order_response_running_{false};

    // Order tracking
    std::map<std::string, proto::OrderEvent> pending_orders_;
    std::mutex orders_mutex_;
    std::queue<std::string> order_response_queue_;
    std::mutex response_queue_mutex_;
    std::condition_variable response_cv_;

    // Test data
    std::vector<std::string> orderbook_snapshots_;
    std::vector<std::string> trade_messages_;
    std::vector<std::string> order_update_templates_;
    std::string position_update_template_;
    std::string balance_update_template_;

    // Internal methods
    void market_data_simulation_loop();
    void order_response_simulation_loop();
    std::string load_json_file(const std::string& filename);
    bool parse_orderbook_snapshot(const std::string& json_data, proto::OrderBookSnapshot& snapshot);
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
};

} // namespace testing
