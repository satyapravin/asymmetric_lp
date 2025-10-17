#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../trader/market_making_strategy.hpp"
#include "../trader/models/glft_target.hpp"
#include "../utils/oms/mock_exchange_oms.hpp"
#include "../utils/oms/order.hpp"
#include "../utils/oms/types.hpp"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

TEST_SUITE("MarketMakingStrategy") {
    
    TEST_CASE("Constructor and Basic Properties") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        CHECK(strategy.get_order_statistics().total_orders == 0);
    }
    
    TEST_CASE("Register Exchange") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        // Should not throw exception
        CHECK(true);
    }
    
    TEST_CASE("Start and Stop Strategy") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        strategy.start();
        
        // Wait a bit for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        strategy.stop();
        
        // Should not throw exception
        CHECK(true);
    }
    
    TEST_CASE("Orderbook Update Triggers Quotes") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        std::vector<OrderEvent> events;
        mock_oms->on_order_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Simulate orderbook update
        std::vector<std::pair<double, double>> bids = {{50000.0, 0.1}};
        std::vector<std::pair<double, double>> asks = {{50001.0, 0.1}};
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        strategy.on_orderbook_update("BTCUSDC-PERP", bids, asks, timestamp);
        
        // Wait for quote generation
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Should have generated quotes (bid and ask orders)
        CHECK(events.size() >= 2);
        
        strategy.stop();
    }
    
    TEST_CASE("Inventory Update Adjusts Quotes") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        std::vector<OrderEvent> events;
        mock_oms->on_order_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Simulate orderbook update to generate initial quotes
        std::vector<std::pair<double, double>> bids = {{50000.0, 0.1}};
        std::vector<std::pair<double, double>> asks = {{50001.0, 0.1}};
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        strategy.on_orderbook_update("BTCUSDC-PERP", bids, asks, timestamp);
        
        // Wait for initial quotes
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        size_t initial_events = events.size();
        
        // Simulate inventory update
        strategy.on_inventory_update("BTCUSDC-PERP", 0.1);
        
        // Wait for quote adjustment
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Should have more events (cancellations and new quotes)
        CHECK(events.size() > initial_events);
        
        strategy.stop();
    }
    
    TEST_CASE("Position Update Handling") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Simulate position update
        strategy.on_position_update("BTCUSDC-PERP", "TEST_EXCHANGE", 0.5, 50000.0);
        
        // Should not throw exception
        CHECK(true);
        
        strategy.stop();
    }
    
    TEST_CASE("Manual Order Submission") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        std::vector<OrderEvent> events;
        mock_oms->on_order_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        Order manual_order;
        manual_order.cl_ord_id = "MANUAL_ORDER_001";
        manual_order.exch = "TEST_EXCHANGE";
        manual_order.symbol = "BTCUSDC-PERP";
        manual_order.side = Side::Buy;
        manual_order.qty = 0.1;
        manual_order.price = 49950.0;
        
        strategy.submit_order(manual_order);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Should have received order event
        CHECK(events.size() > 0);
        CHECK(events[0].cl_ord_id == "MANUAL_ORDER_001");
        
        strategy.stop();
    }
    
    TEST_CASE("Order Cancellation") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Cancel non-existent order - should not crash
        strategy.cancel_order("NON_EXISTENT_ORDER");
        
        CHECK(true); // If we get here, no crash occurred
        
        strategy.stop();
    }
    
    TEST_CASE("Configuration Parameters") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        // Test configuration methods
        strategy.set_inventory_delta(0.5);
        strategy.set_min_spread_bps(10.0);
        strategy.set_max_position_size(2.0);
        strategy.set_quote_size(0.2);
        
        // Should not throw exception
        CHECK(true);
    }
    
    TEST_CASE("Order Statistics") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto stats = strategy.get_order_statistics();
        
        CHECK(stats.total_orders == 0);
        CHECK(stats.filled_orders == 0);
        CHECK(stats.cancelled_orders == 0);
        CHECK(stats.rejected_orders == 0);
        CHECK(stats.total_volume == 0.0);
        CHECK(stats.filled_volume == 0.0);
    }
    
    TEST_CASE("Active Orders") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto active_orders = strategy.get_active_orders();
        
        // Initially should have no active orders
        CHECK(active_orders.size() == 0);
    }
    
    TEST_CASE("Order State Query") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto order_state = strategy.get_order_state("NON_EXISTENT_ORDER");
        
        // Should return empty state for non-existent order
        CHECK(order_state.cl_ord_id.empty());
    }
}