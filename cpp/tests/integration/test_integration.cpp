#include "doctest.h"
#include "../../position_server/position_server_factory.hpp"
#include "../../utils/oms/exchange_oms_factory.hpp"
#include "../../trader/market_making_strategy.hpp"
#include "../../trader/models/glft_target.hpp"
#include "../../utils/oms/order.hpp"
#include "../../utils/oms/types.hpp"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

TEST_SUITE("Integration Tests") {
    
    TEST_CASE("Position Server Integration") {
        // Test that position server factory works with real exchange types
        auto binance_feed = PositionServerFactory::create_from_string("BINANCE", "test_key", "test_secret");
        auto deribit_feed = PositionServerFactory::create_from_string("DERIBIT", "test_client_id", "test_secret");
        auto mock_feed = PositionServerFactory::create_from_string("MOCK", "", "");
        
        REQUIRE(binance_feed != nullptr);
        REQUIRE(deribit_feed != nullptr);
        REQUIRE(mock_feed != nullptr);
        
        // Test connection (mock should work, others will fail without real credentials)
        CHECK(mock_feed->connect("test_account") == true);
        
        mock_feed->disconnect();
    }
    
    TEST_CASE("Exchange OMS Integration") {
        // Test that exchange OMS factory works with different configurations
        ExchangeConfig binance_config;
        binance_config.name = "BINANCE_TEST";
        binance_config.type = "BINANCE";
        binance_config.api_key = "test_key";
        binance_config.api_secret = "test_secret";
        
        ExchangeConfig deribit_config;
        deribit_config.name = "DERIBIT_TEST";
        deribit_config.type = "DERIBIT";
        deribit_config.api_key = "test_client_id";
        deribit_config.api_secret = "test_secret";
        
        ExchangeConfig mock_config;
        mock_config.name = "MOCK_TEST";
        mock_config.type = "MOCK";
        
        auto binance_oms = ExchangeOMSFactory::create_exchange(binance_config);
        auto deribit_oms = ExchangeOMSFactory::create_exchange(deribit_config);
        auto mock_oms = ExchangeOMSFactory::create_exchange(mock_config);
        
        REQUIRE(binance_oms != nullptr);
        REQUIRE(deribit_oms != nullptr);
        REQUIRE(mock_oms != nullptr);
        
        // Test connection (mock should work)
        CHECK(mock_oms->connect().is_success());
        CHECK(mock_oms->is_connected() == true);
        
        mock_oms->disconnect();
    }
    
    TEST_CASE("Market Making Strategy Integration") {
        // Test complete market making strategy with multiple exchanges
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        // Register multiple exchanges
        auto binance_oms = std::make_shared<MockExchangeOMS>("BINANCE", 0.8, 0.1, std::chrono::milliseconds(100));
        auto deribit_oms = std::make_shared<MockExchangeOMS>("DERIBIT", 0.7, 0.15, std::chrono::milliseconds(150));
        auto grvt_oms = std::make_shared<MockExchangeOMS>("GRVT", 0.9, 0.05, std::chrono::milliseconds(80));
        
        strategy.register_exchange("BINANCE", binance_oms);
        strategy.register_exchange("DERIBIT", deribit_oms);
        strategy.register_exchange("GRVT", grvt_oms);
        
        // Set up callbacks
        std::vector<OrderEvent> all_events;
        strategy.set_order_event_callback([&all_events](const OrderEvent& event) {
            all_events.push_back(event);
        });
        
        // Start strategy
        strategy.start();
        
        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Simulate market data
        std::vector<std::pair<double, double>> bids = {{50000.0, 0.1}};
        std::vector<std::pair<double, double>> asks = {{50001.0, 0.1}};
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        strategy.on_orderbook_update("BTCUSDC-PERP", bids, asks, timestamp);
        
        // Wait for quote generation
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Should have generated quotes on the first exchange (simple strategy)
        CHECK(all_events.size() >= 2); // At least 2 quotes (bid + ask) on first exchange
        
        // Simulate inventory update
        strategy.on_inventory_update("BTCUSDC-PERP", 0.1);
        
        // Wait for quote adjustment
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Should have more events (cancellations and new quotes on first exchange)
        CHECK(all_events.size() >= 4);
        
        // Test manual order submission
        Order manual_order;
        manual_order.cl_ord_id = "INTEGRATION_TEST_ORDER";
        manual_order.exch = "BINANCE";
        manual_order.symbol = "BTCUSDC-PERP";
        manual_order.side = Side::Buy;
        manual_order.qty = 0.1;
        manual_order.price = 49950.0;
        
        strategy.submit_order(manual_order);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Should have received the manual order
        bool has_manual_order = false;
        for (const auto& event : all_events) {
            if (event.cl_ord_id == "INTEGRATION_TEST_ORDER") {
                has_manual_order = true;
                break;
            }
        }
        CHECK(has_manual_order);
        
        strategy.stop();
    }
    
    TEST_CASE("End-to-End Order Flow") {
        // Test complete order flow from strategy to exchange
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("ETHUSDC-PERP", glft_model);
        
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 1.0, 0.0, std::chrono::milliseconds(10));
        strategy.register_exchange("TEST_EXCHANGE", mock_oms);
        
        // Connect the exchange to enable order processing
        mock_oms->connect();
        
        std::vector<OrderEvent> events;
        mock_oms->on_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        strategy.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Generate quotes
        std::vector<std::pair<double, double>> bids = {{2000.0, 0.1}};
        std::vector<std::pair<double, double>> asks = {{2001.0, 0.1}};
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        strategy.on_orderbook_update("ETHUSDC-PERP", bids, asks, timestamp);
        
        // Wait for quotes and fills
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Should have acknowledgment and fill events
        bool has_ack = false;
        bool has_fill = false;
        for (const auto& event : events) {
            if (event.type == OrderEventType::Ack) has_ack = true;
            if (event.type == OrderEventType::Fill) has_fill = true;
        }
        
        CHECK(has_ack);
        CHECK(has_fill);
        
        strategy.stop();
    }
    
    TEST_CASE("Multi-Exchange Order Routing") {
        // Test that orders are routed to correct exchanges
        auto glft_model = std::make_shared<GlftTarget>();
        MarketMakingStrategy strategy("BTCUSDC-PERP", glft_model);
        
        auto binance_oms = std::make_shared<MockExchangeOMS>("BINANCE", 0.0, 0.0, std::chrono::milliseconds(10));
        auto deribit_oms = std::make_shared<MockExchangeOMS>("DERIBIT", 0.0, 0.0, std::chrono::milliseconds(10));
        
        strategy.register_exchange("BINANCE", binance_oms);
        strategy.register_exchange("DERIBIT", deribit_oms);
        
        // Connect exchanges to enable order processing
        binance_oms->connect();
        deribit_oms->connect();
        
        std::vector<OrderEvent> binance_events;
        std::vector<OrderEvent> deribit_events;
        
        binance_oms->on_event = [&binance_events](const OrderEvent& event) {
            binance_events.push_back(event);
        };
        
        deribit_oms->on_event = [&deribit_events](const OrderEvent& event) {
            deribit_events.push_back(event);
        };
        
        strategy.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Submit orders to different exchanges
        Order binance_order;
        binance_order.cl_ord_id = "BINANCE_ORDER";
        binance_order.exch = "BINANCE";
        binance_order.symbol = "BTCUSDC-PERP";
        binance_order.side = Side::Buy;
        binance_order.qty = 0.1;
        binance_order.price = 50000.0;
        
        Order deribit_order;
        deribit_order.cl_ord_id = "DERIBIT_ORDER";
        deribit_order.exch = "DERIBIT";
        deribit_order.symbol = "BTCUSDC-PERP";
        deribit_order.side = Side::Sell;
        deribit_order.qty = 0.1;
        deribit_order.price = 50001.0;
        
        strategy.submit_order(binance_order);
        strategy.submit_order(deribit_order);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Orders should be routed to correct exchanges
        CHECK(binance_events.size() > 0);
        CHECK(deribit_events.size() > 0);
        
        bool binance_order_received = false;
        bool deribit_order_received = false;
        
        for (const auto& event : binance_events) {
            if (event.cl_ord_id == "BINANCE_ORDER") {
                binance_order_received = true;
                break;
            }
        }
        
        for (const auto& event : deribit_events) {
            if (event.cl_ord_id == "DERIBIT_ORDER") {
                deribit_order_received = true;
                break;
            }
        }
        
        CHECK(binance_order_received);
        CHECK(deribit_order_received);
        
        strategy.stop();
    }
}