#include "doctest.h"
#include "../../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../../strategies/mm_strategy/market_making_strategy.hpp"
#include "../../../proto/order.pb.h"
#include "../../../proto/market_data.pb.h"
#include "../../../proto/position.pb.h"
#include <memory>
#include <atomic>

TEST_CASE("AbstractStrategy - Basic Functionality") {
    TestStrategy strategy("TestStrategy");
    
    CHECK(strategy.get_name() == "TestStrategy");
    CHECK(!strategy.is_running());
    
    strategy.start();
    CHECK(strategy.is_running());
    
    strategy.stop();
    CHECK(!strategy.is_running());
}

TEST_CASE("AbstractStrategy - Event Handling") {
    TestStrategy strategy("TestStrategy");
    
    // Test order event handling
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id("TEST_ORDER_1");
    order_event.set_symbol("BTCUSDT");
    order_event.set_exch("binance");
    
    strategy.on_order_event(order_event);
    CHECK(true); // Should not crash
    
    // Test market data handling
    proto::OrderBookSnapshot orderbook;
    orderbook.set_symbol("BTCUSDT");
    orderbook.set_exchange("binance");
    
    strategy.on_market_data(orderbook);
    CHECK(true); // Should not crash
    
    // Test position update handling
    proto::PositionUpdate position;
    position.set_symbol("BTCUSDT");
    position.set_exchange("binance");
    position.set_qty(0.1);
    
    strategy.on_position_update(position);
    CHECK(true); // Should not crash
}

TEST_CASE("AbstractStrategy - Query Interface") {
    TestStrategy strategy("TestStrategy");
    
    // Test position queries
    auto position = strategy.get_position("binance", "BTCUSDT");
    CHECK(!position.has_value());
    
    auto all_positions = strategy.get_all_positions();
    CHECK(all_positions.empty());
    
    auto positions_by_exchange = strategy.get_positions_by_exchange("binance");
    CHECK(positions_by_exchange.empty());
    
    auto positions_by_symbol = strategy.get_positions_by_symbol("BTCUSDT");
    CHECK(positions_by_symbol.empty());
    
    // Test balance queries
    auto balance = strategy.get_account_balance("binance", "USDT");
    CHECK(!balance.has_value());
    
    auto all_balances = strategy.get_all_account_balances();
    CHECK(all_balances.empty());
    
    auto balances_by_exchange = strategy.get_account_balances_by_exchange("binance");
    CHECK(balances_by_exchange.empty());
    
    auto balances_by_instrument = strategy.get_account_balances_by_instrument("USDT");
    CHECK(balances_by_instrument.empty());
}

TEST_CASE("MarketMakingStrategy - Initialization") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    CHECK(mm_strategy.get_name() == "MarketMakingStrategy");
    CHECK(!mm_strategy.is_running());
}

TEST_CASE("MarketMakingStrategy - Configuration") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    // Test setting parameters
    mm_strategy.set_spread_bps(10); // 0.1%
    mm_strategy.set_order_size(0.1);
    mm_strategy.set_max_position(1.0);
    
    CHECK(true); // Should not crash
}

TEST_CASE("MarketMakingStrategy - Start and Stop") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    mm_strategy.start();
    CHECK(mm_strategy.is_running());
    
    mm_strategy.stop();
    CHECK(!mm_strategy.is_running());
}

TEST_CASE("MarketMakingStrategy - Market Data Processing") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    // Create orderbook snapshot
    proto::OrderBookSnapshot orderbook;
    orderbook.set_symbol("BTCUSDT");
    orderbook.set_exchange("binance");
    orderbook.set_timestamp_us(1640995200000000);
    
    // Add bid levels
    auto* bid1 = orderbook.add_bids();
    bid1->set_price(49999.0);
    bid1->set_qty(0.1);
    
    auto* bid2 = orderbook.add_bids();
    bid2->set_price(49998.0);
    bid2->set_qty(0.2);
    
    // Add ask levels
    auto* ask1 = orderbook.add_asks();
    ask1->set_price(50001.0);
    ask1->set_qty(0.15);
    
    auto* ask2 = orderbook.add_asks();
    ask2->set_price(50002.0);
    ask2->set_qty(0.25);
    
    // Process market data
    mm_strategy.on_market_data(orderbook);
    CHECK(true); // Should not crash
}

TEST_CASE("MarketMakingStrategy - Order Event Processing") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    // Test order acknowledgment
    proto::OrderEvent ack_event;
    ack_event.set_cl_ord_id("MM_ORDER_1");
    ack_event.set_symbol("BTCUSDT");
    ack_event.set_exch("binance");
    ack_event.set_event_type(proto::OrderEventType::ACK);
    
    mm_strategy.on_order_event(ack_event);
    CHECK(true); // Should not crash
    
    // Test order fill
    proto::OrderEvent fill_event;
    fill_event.set_cl_ord_id("MM_ORDER_1");
    fill_event.set_symbol("BTCUSDT");
    fill_event.set_exch("binance");
    fill_event.set_event_type(proto::OrderEventType::FILL);
    fill_event.set_fill_qty(0.1);
    fill_event.set_fill_price(50000.0);
    
    mm_strategy.on_order_event(fill_event);
    CHECK(true); // Should not crash
}

TEST_CASE("MarketMakingStrategy - Position Management") {
    mm_strategy::MarketMakingStrategy mm_strategy("BTCUSDT", "binance");
    
    // Test position update
    proto::PositionUpdate position;
    position.set_symbol("BTCUSDT");
    position.set_exchange("binance");
    position.set_qty(0.1);
    position.set_avg_price(50000.0);
    
    mm_strategy.on_position_update(position);
    CHECK(true); // Should not crash
}
