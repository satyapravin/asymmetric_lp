#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "../strategies/base_strategy/abstract_strategy.hpp"
#include "../trader/strategy_container.hpp"
#include "../trader/mini_oms.hpp"
#include "../trader/mini_pms.hpp"
#include "../trader/zmq_oms_adapter.hpp"
#include "../trader/zmq_mds_adapter.hpp"
#include "../trader/zmq_pms_adapter.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"

namespace trader {

/**
 * Trader Library
 * 
 * Core trading strategy execution logic that can be used as:
 * 1. Library for testing and integration
 * 2. Standalone process for production deployment
 * 
 * Responsibilities:
 * - Manage strategy container and strategy lifecycle
 * - Handle ZMQ communication with servers
 * - Coordinate order management via Mini OMS
 * - Coordinate position management via Mini PMS
 * - Execute trading strategies
 */
class TraderLib {
public:
    TraderLib();
    ~TraderLib();

    // Library lifecycle
    bool initialize(const std::string& config_file = "");
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void set_strategy(std::shared_ptr<AbstractStrategy> strategy);
    void set_symbol(const std::string& symbol) { symbol_ = symbol; }
    void set_exchange(const std::string& exchange) { exchange_ = exchange; }

    // ZMQ adapter setup
    void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) { oms_adapter_ = adapter; }
    void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) { mds_adapter_ = adapter; }
    void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) { pms_adapter_ = adapter; }

    // Event callbacks for testing
    using OrderEventCallback = std::function<void(const proto::OrderEvent&)>;
    using MarketDataCallback = std::function<void(const proto::OrderBookSnapshot&)>;
    using PositionUpdateCallback = std::function<void(const proto::PositionUpdate&)>;
    using BalanceUpdateCallback = std::function<void(const proto::AccountBalanceUpdate&)>;
    using TradeExecutionCallback = std::function<void(const proto::Trade&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void set_order_event_callback(OrderEventCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        order_event_callback_ = callback;
    }
    void set_market_data_callback(MarketDataCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        market_data_callback_ = callback;
    }
    void set_position_update_callback(PositionUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        position_update_callback_ = callback;
    }
    void set_balance_update_callback(BalanceUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        balance_update_callback_ = callback;
    }
    void set_trade_execution_callback(TradeExecutionCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        trade_execution_callback_ = callback;
    }
    void set_error_callback(ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        error_callback_ = callback;
    }

    // Strategy management
    std::shared_ptr<AbstractStrategy> get_strategy() const;
    bool has_strategy() const;
    void reload_strategy();

    // Order management interface
    bool send_order(const std::string& cl_ord_id, const std::string& symbol, 
                   proto::Side side, proto::OrderType type, double qty, double price);
    bool cancel_order(const std::string& cl_ord_id);
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty);

    // Order state queries
    std::optional<OrderStateInfo> get_order_state(const std::string& cl_ord_id) const;
    std::vector<OrderStateInfo> get_active_orders() const;
    std::vector<OrderStateInfo> get_all_orders() const;

    // Position queries
    std::optional<PositionInfo> get_position(const std::string& exchange, const std::string& symbol) const;
    std::vector<PositionInfo> get_all_positions() const;
    std::vector<PositionInfo> get_positions_by_exchange(const std::string& exchange) const;
    std::vector<PositionInfo> get_positions_by_symbol(const std::string& symbol) const;

    // Balance queries
    std::optional<AccountBalanceInfo> get_account_balance(const std::string& exchange, const std::string& instrument) const;
    std::vector<AccountBalanceInfo> get_all_account_balances() const;
    std::vector<AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const;
    std::vector<AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const;

    // Statistics
    struct Statistics {
        std::atomic<uint64_t> orders_sent{0};
        std::atomic<uint64_t> orders_cancelled{0};
        std::atomic<uint64_t> orders_modified{0};
        std::atomic<uint64_t> market_data_received{0};
        std::atomic<uint64_t> position_updates{0};
        std::atomic<uint64_t> balance_updates{0};
        std::atomic<uint64_t> trade_executions{0};
        std::atomic<uint64_t> zmq_messages_received{0};
        std::atomic<uint64_t> zmq_messages_sent{0};
        std::atomic<uint64_t> strategy_errors{0};
        
        void reset() {
            orders_sent.store(0);
            orders_cancelled.store(0);
            orders_modified.store(0);
            market_data_received.store(0);
            position_updates.store(0);
            balance_updates.store(0);
            trade_executions.store(0);
            zmq_messages_received.store(0);
            zmq_messages_sent.store(0);
            strategy_errors.store(0);
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }

    // Testing interface
    void simulate_market_data(const proto::OrderBookSnapshot& orderbook);
    void simulate_order_event(const proto::OrderEvent& order_event);
    void simulate_position_update(const proto::PositionUpdate& position);
    void simulate_balance_update(const proto::AccountBalanceUpdate& balance);
    void simulate_trade_execution(const proto::Trade& trade);

private:
    std::atomic<bool> running_;
    std::string symbol_;
    std::string exchange_;
    
    // Core components
    std::unique_ptr<config::ProcessConfigManager> config_manager_;
    std::unique_ptr<StrategyContainer> strategy_container_;
    std::unique_ptr<MiniOMS> mini_oms_;
    std::unique_ptr<MiniPMS> mini_pms_;
    
    // ZMQ adapters
    std::shared_ptr<ZmqOMSAdapter> oms_adapter_;
    std::shared_ptr<ZmqMDSAdapter> mds_adapter_;
    std::shared_ptr<ZmqPMSAdapter> pms_adapter_;
    
    // OMS event polling thread
    std::atomic<bool> oms_event_running_;
    std::thread oms_event_thread_;
    
    // Strategy
    std::shared_ptr<AbstractStrategy> strategy_;
    
    // Callbacks (protected by mutex for thread safety)
    mutable std::mutex callback_mutex_;
    OrderEventCallback order_event_callback_;
    MarketDataCallback market_data_callback_;
    PositionUpdateCallback position_update_callback_;
    BalanceUpdateCallback balance_update_callback_;
    TradeExecutionCallback trade_execution_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    Statistics statistics_;
    
    // Internal methods
    void setup_strategy_container();
    void setup_zmq_adapters();
    void handle_order_event(const proto::OrderEvent& order_event);
    void handle_market_data(const proto::OrderBookSnapshot& orderbook);
    void handle_position_update(const proto::PositionUpdate& position);
    void handle_balance_update(const proto::AccountBalanceUpdate& balance);
    void handle_trade_execution(const proto::Trade& trade);
    void handle_error(const std::string& error_message);
};

} // namespace trader
