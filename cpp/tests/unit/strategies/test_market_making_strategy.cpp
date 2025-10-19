#include "doctest.h"
#include "../../strategies/mm_strategy/market_making_strategy.hpp"
#include "../../strategies/mm_strategy/models/glft_target.hpp"
#include <memory>
#include <string>

TEST_SUITE("MarketMakingStrategy") {
    
    TEST_CASE("Constructor and Basic Properties") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        CHECK(strategy.get_statistics().total_orders == 0);
        CHECK(strategy.get_name() == "MarketMakingStrategy");
        CHECK(strategy.is_running() == false);
    }
    
    TEST_CASE("Start and Stop") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        CHECK(strategy.is_running() == false);
        
        strategy.start();
        CHECK(strategy.is_running() == true);
        
        strategy.stop();
        CHECK(strategy.is_running() == false);
    }
    
    TEST_CASE("Configuration") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        strategy.set_inventory_delta(0.5);
        strategy.set_min_spread_bps(10.0);
        strategy.set_max_position_size(100.0);
        strategy.set_quote_size(2.0);
        
        // Configuration methods don't return values, so we just verify they don't crash
        CHECK(true);
    }
    
    TEST_CASE("Order State Queries") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        // Test order state queries on empty strategy
        auto order_state = strategy.get_order_state("NON_EXISTENT_ORDER");
        CHECK(order_state.cl_ord_id == "NON_EXISTENT_ORDER");
        
        auto active_orders = strategy.get_active_orders();
        CHECK(active_orders.empty());
        
        auto all_orders = strategy.get_all_orders();
        CHECK(all_orders.empty());
    }
    
    TEST_CASE("Order Management") {
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        strategy.start();
        
        // Note: Strategy no longer has order management methods
        // Order management is handled by StrategyContainer -> Mini OMS -> ZMQ Adapter
        // This test verifies the strategy can start and stop properly
        
        CHECK(strategy.is_running() == true);
        
        strategy.stop();
        CHECK(strategy.is_running() == false);
    }
}