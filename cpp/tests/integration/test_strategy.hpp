#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <map>
#include "../strategies/base_strategy/abstract_strategy.hpp"
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"
#include "../proto/position.pb.h"
#include "../proto/acc_balance.pb.h"

namespace testing {

/**
 * Test Strategy for End-to-End Testing
 * 
 * This strategy exercises all order lifecycle scenarios:
 * - Order submission and ACK
 * - Partial fills
 * - Complete fills
 * - Order cancellations
 * - Order rejections
 * - Position updates
 * - Balance updates
 */
class TestStrategy : public AbstractStrategy {
public:
    TestStrategy();
    ~TestStrategy() = default;

    // AbstractStrategy interface implementation
    void start() override;
    void stop() override;
    bool is_running() const override { return running_.load(); }

    // Event handlers
    void on_market_data(const proto::OrderBookSnapshot& orderbook) override;
    void on_order_event(const proto::OrderEvent& order_event) override;
    void on_position_update(const proto::PositionUpdate& position) override;
    void on_trade_execution(const proto::Trade& trade) override;
    void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) override;

    // Test configuration
    void set_test_scenarios(const std::vector<std::string>& scenarios);
    void set_order_quantity(double qty) { order_quantity_ = qty; }
    void set_order_price(double price) { order_price_ = price; }

    // Test results
    struct TestResults {
        std::atomic<uint64_t> orders_sent{0};
        std::atomic<uint64_t> orders_acked{0};
        std::atomic<uint64_t> orders_rejected{0};
        std::atomic<uint64_t> orders_cancelled{0};
        std::atomic<uint64_t> orders_partial_filled{0};
        std::atomic<uint64_t> orders_filled{0};
        std::atomic<uint64_t> market_data_received{0};
        std::atomic<uint64_t> position_updates{0};
        std::atomic<uint64_t> balance_updates{0};
        std::atomic<uint64_t> trade_executions{0};
        
        std::map<std::string, std::chrono::system_clock::time_point> order_timestamps;
        std::map<std::string, proto::OrderStatus> order_statuses;
        
        void reset() {
            orders_sent.store(0);
            orders_acked.store(0);
            orders_rejected.store(0);
            orders_cancelled.store(0);
            orders_partial_filled.store(0);
            orders_filled.store(0);
            market_data_received.store(0);
            position_updates.store(0);
            balance_updates.store(0);
            trade_executions.store(0);
            order_timestamps.clear();
            order_statuses.clear();
        }
    };

    const TestResults& get_test_results() const { return test_results_; }
    void reset_test_results() { test_results_.reset(); }

    // Test scenarios
    void run_basic_order_test();
    void run_partial_fill_test();
    void run_cancellation_test();
    void run_rejection_test();
    void run_market_data_test();

private:
    std::atomic<bool> running_;
    std::vector<std::string> test_scenarios_;
    double order_quantity_{0.1};
    double order_price_{50000.0};
    
    TestResults test_results_;
    std::mutex results_mutex_;
    
    // Test state tracking
    std::map<std::string, std::chrono::system_clock::time_point> pending_orders_;
    std::atomic<uint64_t> order_counter_{0};
    
    // Helper methods
    std::string generate_test_order_id();
    void log_test_event(const std::string& event, const std::string& details = "");
    bool wait_for_order_status(const std::string& order_id, proto::OrderStatus expected_status, 
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Order management (delegated to container)
    bool send_test_order(const std::string& cl_ord_id, proto::Side side, proto::OrderType type, 
                        double qty, double price);
    bool cancel_test_order(const std::string& cl_ord_id);
};

} // namespace testing
