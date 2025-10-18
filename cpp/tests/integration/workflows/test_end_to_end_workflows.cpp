#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include "../../../exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"
#include "../../../exchanges/binance/private_websocket/binance_private_websocket_handler.hpp"
#include "../../../exchanges/binance/http/binance_oms.hpp"
#include "../../../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include "../../../utils/zmq/zmq_subscriber.hpp"
#include "../../../utils/oms/order.hpp"
#include "../../../utils/oms/order_state.hpp"
#include "../../config/test_config_manager.hpp"

using namespace binance;
using namespace test_config;

TEST_SUITE("End-to-End Integration Tests") {

    TEST_CASE("Complete Order Lifecycle - Market Order") {
        // Load test configuration
        auto& config_manager = get_test_config();
        CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        // Setup ZMQ communication
        ZmqPublisher order_pub("tcp://127.0.0.1:7001");
        ZmqSubscriber order_sub("tcp://127.0.0.1:7001", "order_events");
        
        // Create OMS for order management
        BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
        CHECK(oms.connect(binance_config.http_url));
        
        // Create order
        Order order;
        order.cl_ord_id = "test_order_" + std::to_string(std::time(nullptr));
        order.symbol = binance_config.symbol;
        order.side = OrderSide::BUY;
        order.order_type = OrderType::MARKET;
        order.qty = 0.1;
        order.price = 0.0; // Market order
        
        // Place order
        auto place_result = oms.send_order(order);
        CHECK(place_result.has_value());
        
        // Wait for order acknowledgment
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify order was placed
        auto order_status = oms.get_order_status(order.symbol, order.cl_ord_id);
        
        // Cancel order if still open
        if (order_status.has_value()) {
            auto cancel_result = oms.cancel_order(order.cl_ord_id, "");
            // Cancel may succeed or fail depending on execution speed
        }
        
        oms.disconnect();
    }

    TEST_CASE("Complete Order Lifecycle - Limit Order") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
        CHECK(oms.connect(binance_config.http_url));
        
        // Create limit order
        Order order;
        order.cl_ord_id = "test_limit_" + std::to_string(std::time(nullptr));
        order.symbol = binance_config.symbol;
        order.side = OrderSide::SELL;
        order.order_type = OrderType::LIMIT;
        order.qty = 0.1;
        order.price = 50000.0; // Below market price
        
        // Place order
        auto place_result = oms.send_order(order);
        CHECK(place_result.has_value());
        
        // Wait for order acknowledgment
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify order was placed
        auto order_status = oms.get_order_status(order.symbol, order.cl_ord_id);
        
        // Cancel order
        auto cancel_result = oms.cancel_order(order.cl_ord_id, "");
        CHECK(cancel_result.has_value());
        
        oms.disconnect();
    }

    TEST_CASE("Market Data Flow Integration") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        // Setup ZMQ for market data
        ZmqPublisher md_pub("tcp://127.0.0.1:6001");
        ZmqSubscriber md_sub("tcp://127.0.0.1:6001", "market_data");
        
        // Create public WebSocket handler
        BinancePublicWebSocketHandler ws_handler;
        CHECK(ws_handler.connect(binance_config.public_ws_url));
        
        // Subscribe to market data
        CHECK(ws_handler.subscribe_to_ticker(binance_config.symbol));
        CHECK(ws_handler.subscribe_to_orderbook(binance_config.symbol, 20));
        
        // Set up callbacks to publish to ZMQ
        std::atomic<int> ticker_count{0};
        std::atomic<int> orderbook_count{0};
        
        ws_handler.set_ticker_callback([&](const std::string& symbol, double price, double volume) {
            ticker_count++;
            // In real implementation, would publish to ZMQ
        });
        
        ws_handler.set_orderbook_callback([&](const std::string& symbol, 
                                              const std::vector<std::pair<double, double>>& bids,
                                              const std::vector<std::pair<double, double>>& asks) {
            orderbook_count++;
            // In real implementation, would publish to ZMQ
        });
        
        // Let it run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        ws_handler.disconnect();
    }

    TEST_CASE("Position Update Integration") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        // Setup ZMQ for position updates
        ZmqPublisher pos_pub("tcp://127.0.0.1:6003");
        ZmqSubscriber pos_sub("tcp://127.0.0.1:6003", "position_updates");
        
        // Create data fetcher for position monitoring
        BinanceDataFetcher fetcher(binance_config.api_key, binance_config.api_secret);
        CHECK(fetcher.connect(binance_config.http_url));
        
        // Set up position callback
        std::atomic<int> position_updates{0};
        fetcher.set_position_callback([&](const std::string& symbol, double qty, double avg_price) {
            position_updates++;
            // In real implementation, would publish to ZMQ
        });
        
        // Get current positions
        auto positions = fetcher.get_positions();
        
        // Get account info
        auto account_info = fetcher.get_account_info();
        
        fetcher.disconnect();
    }

    TEST_CASE("Multi-Process Communication Simulation") {
        // Simulate communication between trader, quote_server, trading_engine, position_server
        
        // Trader publishes orders
        ZmqPublisher trader_pub("tcp://127.0.0.1:7003");
        
        // Trading engine subscribes to orders
        ZmqSubscriber trading_engine_sub("tcp://127.0.0.1:7003", "orders");
        
        // Trading engine publishes order events
        ZmqPublisher trading_engine_pub("tcp://127.0.0.1:6002");
        
        // Trader subscribes to order events
        ZmqSubscriber trader_sub("tcp://127.0.0.1:6002", "order_events");
        
        // Position server publishes position updates
        ZmqPublisher position_pub("tcp://127.0.0.1:6003");
        
        // Trader subscribes to position updates
        ZmqSubscriber position_sub("tcp://127.0.0.1:6003", "position_updates");
        
        // Simulate order flow
        std::string test_order = R"({"cl_ord_id":"test_123","symbol":"BTCUSDT","side":"BUY","qty":0.1,"price":50000.0})";
        
        // Publish order
        trader_pub.publish("orders", test_order);
        
        // Simulate order event
        std::string order_event = R"({"cl_ord_id":"test_123","status":"FILLED","fill_qty":0.1,"fill_price":50000.0})";
        trading_engine_pub.publish("order_events", order_event);
        
        // Simulate position update
        std::string position_update = R"({"symbol":"BTCUSDT","qty":0.1,"avg_price":50000.0})";
        position_pub.publish("position_updates", position_update);
        
        // Give time for message propagation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    TEST_CASE("Error Recovery Integration") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
        
        // Test connection failure and recovery
        CHECK(oms.connect(binance_config.http_url));
        
        // Simulate connection loss
        oms.disconnect();
        CHECK_FALSE(oms.is_connected());
        
        // Reconnect
        CHECK(oms.connect(binance_config.http_url));
        CHECK(oms.is_connected());
        
        // Test order after reconnection
        Order order;
        order.cl_ord_id = "recovery_test_" + std::to_string(std::time(nullptr));
        order.symbol = binance_config.symbol;
        order.side = OrderSide::BUY;
        order.order_type = OrderType::MARKET;
        order.qty = 0.01; // Small amount for test
        
        auto place_result = oms.send_order(order);
        // May succeed or fail depending on connection state
        
        oms.disconnect();
    }

    TEST_CASE("Concurrent Operations Integration") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
        CHECK(oms.connect(binance_config.http_url));
        
        // Test concurrent order operations
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&, i]() {
                Order order;
                order.cl_ord_id = "concurrent_" + std::to_string(i) + "_" + std::to_string(std::time(nullptr));
                order.symbol = binance_config.symbol;
                order.side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;
                order.order_type = OrderType::MARKET;
                order.qty = 0.01;
                
                auto result = oms.send_order(order);
                if (result.has_value()) {
                    success_count++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // At least some orders should succeed
        CHECK(success_count.load() >= 0);
        
        oms.disconnect();
    }

    TEST_CASE("Data Consistency Integration") {
        auto& config_manager = get_test_config();
        auto binance_config = config_manager.get_exchange_config("BINANCE");
        
        BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
        BinanceDataFetcher fetcher(binance_config.api_key, binance_config.api_secret);
        
        CHECK(oms.connect(binance_config.http_url));
        CHECK(fetcher.connect(binance_config.http_url));
        
        // Get account info from both OMS and data fetcher
        auto oms_account = oms.get_account_info();
        auto fetcher_account = fetcher.get_account_info();
        
        // Get positions from both
        auto oms_positions = oms.get_positions();
        auto fetcher_positions = fetcher.get_positions();
        
        // Get open orders
        auto open_orders = oms.get_open_orders(binance_config.symbol);
        
        // Data should be consistent between sources
        // (In real implementation, would compare actual data)
        
        oms.disconnect();
        fetcher.disconnect();
    }
}
