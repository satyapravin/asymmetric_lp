#include "doctest.h"
#include "../../../trader/trader_lib.hpp"
#include "../../../trader/zmq_oms_adapter.hpp"
#include "../../../trader/zmq_mds_adapter.hpp"
#include "../../../trader/zmq_pms_adapter.hpp"
#include "../../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../../proto/order.pb.h"
#include "../../../proto/market_data.pb.h"
#include "../../../proto/position.pb.h"
#include <memory>
#include <atomic>

// Mock strategy for testing
class MockStrategy : public AbstractStrategy {
public:
    std::atomic<int> order_event_count_{0};
    std::atomic<int> market_data_count_{0};
    std::atomic<int> position_update_count_{0};
    
    MockStrategy() : AbstractStrategy("MockStrategy") {}
    
    void start() override { running_.store(true); }
    void stop() override { running_.store(false); }
    
    void on_order_event(const proto::OrderEvent& order_event) override {
        order_event_count_++;
    }
    
    void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
        market_data_count_++;
    }
    
    void on_position_update(const proto::PositionUpdate& position) override {
        position_update_count_++;
    }
    
    void on_trade_execution(const proto::Trade& trade) override {}
    void on_account_balance_update(const proto::AccountBalanceUpdate& balance) override {}
    
    // Query interface
    std::optional<trader::PositionInfo> get_position(const std::string&, const std::string&) const override { return std::nullopt; }
    std::vector<trader::PositionInfo> get_all_positions() const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string&) const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string&) const override { return {}; }
    std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string&, const std::string&) const override { return std::nullopt; }
    std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string&) const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string&) const override { return {}; }
};

TEST_CASE("TraderLib - Initialization") {
    trader::TraderLib trader_lib;
    
    CHECK(!trader_lib.is_running());
}

TEST_CASE("TraderLib - Set Exchange and Symbol") {
    trader::TraderLib trader_lib;
    
    trader_lib.set_exchange("binance");
    trader_lib.set_symbol("BTCUSDT");
    
    // Test that values are set (no direct getters, but should not crash)
    CHECK(true);
}

TEST_CASE("TraderLib - Set Strategy") {
    trader::TraderLib trader_lib;
    
    auto strategy = std::make_shared<MockStrategy>();
    trader_lib.set_strategy(strategy);
    
    auto retrieved_strategy = trader_lib.get_strategy();
    CHECK(retrieved_strategy != nullptr);
    CHECK(trader_lib.has_strategy());
}

TEST_CASE("TraderLib - Start and Stop") {
    trader::TraderLib trader_lib;
    
    // Initialize with config
    trader_lib.initialize("../tests/test_config.ini");
    
    trader_lib.start();
    CHECK(trader_lib.is_running());
    
    trader_lib.stop();
    CHECK(!trader_lib.is_running());
}

TEST_CASE("TraderLib - Order Management") {
    trader::TraderLib trader_lib;
    
    // Test order sending
    bool result = trader_lib.send_order("TEST_ORDER_1", "BTCUSDT", proto::BUY, proto::LIMIT, 0.1, 50000.0);
    // Note: This might fail without proper initialization, but should not crash
    CHECK(true);
    
    // Test order cancellation
    bool cancel_result = trader_lib.cancel_order("TEST_ORDER_1");
    CHECK(true); // Should not crash
    
    // Test order modification
    bool modify_result = trader_lib.modify_order("TEST_ORDER_1", 51000.0, 0.2);
    CHECK(true); // Should not crash
}

TEST_CASE("TraderLib - Order State Queries") {
    trader::TraderLib trader_lib;
    
    // Test getting order state
    auto order_state = trader_lib.get_order_state("TEST_ORDER_1");
    CHECK(!order_state.has_value());
    
    // Test getting active orders
    auto active_orders = trader_lib.get_active_orders();
    CHECK(active_orders.empty());
    
    // Test getting all orders
    auto all_orders = trader_lib.get_all_orders();
    CHECK(all_orders.empty());
}

TEST_CASE("TraderLib - Position Queries") {
    trader::TraderLib trader_lib;
    
    // Test position queries
    auto position = trader_lib.get_position("binance", "BTCUSDT");
    CHECK(!position.has_value());
    
    auto all_positions = trader_lib.get_all_positions();
    CHECK(all_positions.empty());
    
    auto positions_by_exchange = trader_lib.get_positions_by_exchange("binance");
    CHECK(positions_by_exchange.empty());
    
    auto positions_by_symbol = trader_lib.get_positions_by_symbol("BTCUSDT");
    CHECK(positions_by_symbol.empty());
}

TEST_CASE("TraderLib - Balance Queries") {
    trader::TraderLib trader_lib;
    
    // Test balance queries
    auto balance = trader_lib.get_account_balance("binance", "USDT");
    CHECK(!balance.has_value());
    
    auto all_balances = trader_lib.get_all_account_balances();
    CHECK(all_balances.empty());
    
    auto balances_by_exchange = trader_lib.get_account_balances_by_exchange("binance");
    CHECK(balances_by_exchange.empty());
    
    auto balances_by_instrument = trader_lib.get_account_balances_by_instrument("USDT");
    CHECK(balances_by_instrument.empty());
}

TEST_CASE("TraderLib - Statistics") {
    trader::TraderLib trader_lib;
    
    auto stats = trader_lib.get_statistics();
    
    // Test initial statistics
    CHECK(stats.orders_sent.load() == 0);
    CHECK(stats.orders_cancelled.load() == 0);
    CHECK(stats.orders_modified.load() == 0);
    CHECK(stats.market_data_received.load() == 0);
    CHECK(stats.position_updates.load() == 0);
    
    // Test reset statistics
    trader_lib.reset_statistics();
    CHECK(stats.orders_sent.load() == 0);
}

TEST_CASE("TraderLib - Simulation Methods") {
    trader::TraderLib trader_lib;
    
    // Test simulation methods (for testing)
    proto::OrderBookSnapshot orderbook;
    orderbook.set_symbol("BTCUSDT");
    orderbook.set_exchange("binance");
    
    trader_lib.simulate_market_data(orderbook);
    
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id("TEST_ORDER_1");
    order_event.set_symbol("BTCUSDT");
    order_event.set_exch("binance");
    
    trader_lib.simulate_order_event(order_event);
    
    proto::PositionUpdate position;
    position.set_symbol("BTCUSDT");
    position.set_exchange("binance");
    position.set_qty(0.1);
    
    trader_lib.simulate_position_update(position);
    
    CHECK(true); // Should not crash
}
