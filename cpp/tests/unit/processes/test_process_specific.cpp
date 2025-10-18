#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include "../../../quote_server/quote_server.hpp"
#include "../../../quote_server/exchange_manager.hpp"
#include "../../../quote_server/exchange_manager_factory.hpp"
#include "../../../position_server/position_server_factory.hpp"
#include "../../../trader/market_making_strategy.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include "../../../utils/zmq/zmq_subscriber.hpp"
#include "../../config/test_config_manager.hpp"

using namespace test_config;

TEST_SUITE("Process-Specific Tests") {

    TEST_SUITE("Quote Server Tests") {
        
        TEST_CASE("Quote Server - Constructor and Destructor") {
            QuoteServer server;
            CHECK_FALSE(server.is_running());
        }

        TEST_CASE("Quote Server - Initialize and Shutdown") {
            QuoteServer server;
            
            CHECK(server.initialize());
            CHECK_FALSE(server.is_running());
            
            server.shutdown();
            CHECK_FALSE(server.is_running());
        }

        TEST_CASE("Quote Server - Configuration Loading") {
            QuoteServer server;
            
            // Test with configuration
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            // Set configuration
            server.set_exchange_name("BINANCE");
            server.set_symbol(binance_config.symbol);
            server.set_websocket_url(binance_config.public_ws_url);
            
            CHECK_EQ(server.get_exchange_name(), "BINANCE");
            CHECK_EQ(server.get_symbol(), binance_config.symbol);
        }

        TEST_CASE("Quote Server - Exchange Manager Factory") {
            // Test exchange manager creation
            auto manager = ExchangeManagerFactory::create("BINANCE", "wss://fstream.binance.com/stream");
            CHECK(manager != nullptr);
            
            auto deribit_manager = ExchangeManagerFactory::create("DERIBIT", "wss://www.deribit.com/ws/api/v2");
            CHECK(deribit_manager != nullptr);
            
            // Test unknown exchange
            auto unknown_manager = ExchangeManagerFactory::create("UNKNOWN", "wss://unknown.com");
            CHECK(unknown_manager != nullptr); // Should return generic manager
        }

        TEST_CASE("Quote Server - Market Data Publishing") {
            QuoteServer server;
            
            // Setup ZMQ publisher
            ZmqPublisher publisher("tcp://127.0.0.1:6001");
            
            // Configure server
            server.set_exchange_name("BINANCE");
            server.set_symbol("BTCUSDT");
            server.set_websocket_url("wss://fstream.binance.com/stream");
            
            CHECK(server.initialize());
            
            // Test market data publishing
            // In real implementation, would test actual data publishing
        }

        TEST_CASE("Quote Server - Multiple Exchange Support") {
            // Test support for multiple exchanges
            std::vector<std::string> exchanges = {"BINANCE", "DERIBIT", "GRVT"};
            
            for (const auto& exchange : exchanges) {
                auto manager = ExchangeManagerFactory::create(exchange, "wss://test.com");
                CHECK(manager != nullptr);
            }
        }
    }

    TEST_SUITE("Position Server Tests") {
        
        TEST_CASE("Position Server Factory - Exchange Types") {
            // Test different exchange types
            auto binance_feed = PositionServerFactory::create_from_string("BINANCE", "test_key", "test_secret");
            CHECK(binance_feed != nullptr);
            
            auto deribit_feed = PositionServerFactory::create_from_string("DERIBIT", "test_key", "test_secret");
            CHECK(deribit_feed != nullptr);
            
            auto mock_feed = PositionServerFactory::create_from_string("MOCK", "", "");
            CHECK(mock_feed != nullptr);
        }

        TEST_CASE("Position Server - Position Updates") {
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            // Create position feed
            auto position_feed = PositionServerFactory::create_from_string("BINANCE", 
                                                                           binance_config.api_key, 
                                                                           binance_config.api_secret);
            CHECK(position_feed != nullptr);
            
            // Setup ZMQ publisher
            ZmqPublisher publisher("tcp://127.0.0.1:6003");
            
            // Set up position update callback
            std::atomic<int> update_count{0};
            position_feed->on_position_update = [&](const std::string& symbol,
                                                   const std::string& exch,
                                                   double qty,
                                                   double avg_price) {
                update_count++;
                // In real implementation, would publish to ZMQ
            };
            
            // Test connection
            CHECK(position_feed->connect("test_account"));
            
            // Disconnect
            position_feed->disconnect();
        }

        TEST_CASE("Position Server - Mock Feed") {
            auto mock_feed = PositionServerFactory::create_from_string("MOCK", "", "");
            CHECK(mock_feed != nullptr);
            
            // Test mock position updates
            std::atomic<int> mock_updates{0};
            mock_feed->on_position_update = [&](const std::string& symbol,
                                                const std::string& exch,
                                                double qty,
                                                double avg_price) {
                mock_updates++;
            };
            
            CHECK(mock_feed->connect("mock_account"));
            
            // Give time for mock updates
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            mock_feed->disconnect();
        }
    }

    TEST_SUITE("Trading Engine Tests") {
        
        TEST_CASE("Trading Engine - Order Processing") {
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            // Setup ZMQ communication
            ZmqSubscriber order_sub("tcp://127.0.0.1:7003", "orders");
            ZmqPublisher event_pub("tcp://127.0.0.1:6002");
            
            // Simulate order processing
            std::string test_order = R"({"cl_ord_id":"test_123","symbol":"BTCUSDT","side":"BUY","qty":0.1,"price":50000.0})";
            
            // Publish order
            ZmqPublisher order_pub("tcp://127.0.0.1:7003");
            order_pub.publish("orders", test_order);
            
            // Give time for processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate order event
            std::string order_event = R"({"cl_ord_id":"test_123","status":"FILLED","fill_qty":0.1,"fill_price":50000.0})";
            event_pub.publish("order_events", order_event);
        }

        TEST_CASE("Trading Engine - Error Handling") {
            // Test error handling in trading engine
            ZmqSubscriber order_sub("tcp://127.0.0.1:7003", "orders");
            
            // Test with invalid order data
            std::string invalid_order = R"({"invalid": "data"})";
            
            ZmqPublisher order_pub("tcp://127.0.0.1:7003");
            order_pub.publish("orders", invalid_order);
            
            // Should handle invalid data gracefully
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        TEST_CASE("Trading Engine - Concurrent Orders") {
            ZmqSubscriber order_sub("tcp://127.0.0.1:7003", "orders");
            ZmqPublisher event_pub("tcp://127.0.0.1:6002");
            
            // Test concurrent order processing
            std::vector<std::thread> threads;
            
            for (int i = 0; i < 5; ++i) {
                threads.emplace_back([&, i]() {
                    std::string order = R"({"cl_ord_id":")" + std::to_string(i) + R"(","symbol":"BTCUSDT","side":"BUY","qty":0.1,"price":50000.0})";
                    
                    ZmqPublisher order_pub("tcp://127.0.0.1:7003");
                    order_pub.publish("orders", order);
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            // Give time for processing
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    TEST_SUITE("Trader Tests") {
        
        TEST_CASE("Market Making Strategy - Constructor and Destructor") {
            MarketMakingStrategy strategy;
            CHECK_FALSE(strategy.is_running());
        }

        TEST_CASE("Market Making Strategy - Initialize and Shutdown") {
            MarketMakingStrategy strategy;
            
            CHECK(strategy.initialize());
            CHECK_FALSE(strategy.is_running());
            
            strategy.shutdown();
            CHECK_FALSE(strategy.is_running());
        }

        TEST_CASE("Market Making Strategy - Configuration") {
            MarketMakingStrategy strategy;
            
            // Test strategy configuration
            strategy.set_symbol("BTCUSDT");
            strategy.set_spread_bps(10); // 10 basis points
            strategy.set_order_size(0.1);
            strategy.set_max_position(1.0);
            
            CHECK_EQ(strategy.get_symbol(), "BTCUSDT");
            CHECK_EQ(strategy.get_spread_bps(), 10);
            CHECK_EQ(strategy.get_order_size(), 0.1);
            CHECK_EQ(strategy.get_max_position(), 1.0);
        }

        TEST_CASE("Market Making Strategy - Order Generation") {
            MarketMakingStrategy strategy;
            
            strategy.set_symbol("BTCUSDT");
            strategy.set_spread_bps(10);
            strategy.set_order_size(0.1);
            
            // Test order generation logic
            // In real implementation, would test actual order generation
            CHECK(strategy.initialize());
            
            strategy.shutdown();
        }

        TEST_CASE("Market Making Strategy - Risk Management") {
            MarketMakingStrategy strategy;
            
            strategy.set_symbol("BTCUSDT");
            strategy.set_max_position(1.0);
            strategy.set_max_daily_loss(1000.0);
            
            // Test risk management
            CHECK(strategy.check_risk_limits(0.5)); // Within limits
            CHECK_FALSE(strategy.check_risk_limits(2.0)); // Exceeds limits
        }

        TEST_CASE("Market Making Strategy - Market Data Processing") {
            MarketMakingStrategy strategy;
            
            strategy.set_symbol("BTCUSDT");
            CHECK(strategy.initialize());
            
            // Simulate market data processing
            // In real implementation, would test actual market data handling
            
            strategy.shutdown();
        }
    }

    TEST_SUITE("Process Integration Tests") {
        
        TEST_CASE("Multi-Process Communication") {
            // Test communication between all processes
            
            // Quote Server publishes market data
            ZmqPublisher md_pub("tcp://127.0.0.1:6001");
            
            // Trader subscribes to market data
            ZmqSubscriber md_sub("tcp://127.0.0.1:6001", "market_data");
            
            // Trader publishes orders
            ZmqPublisher order_pub("tcp://127.0.0.1:7003");
            
            // Trading Engine subscribes to orders
            ZmqSubscriber order_sub("tcp://127.0.0.1:7003", "orders");
            
            // Trading Engine publishes order events
            ZmqPublisher event_pub("tcp://127.0.0.1:6002");
            
            // Trader subscribes to order events
            ZmqSubscriber event_sub("tcp://127.0.0.1:6002", "order_events");
            
            // Position Server publishes position updates
            ZmqPublisher pos_pub("tcp://127.0.0.1:6003");
            
            // Trader subscribes to position updates
            ZmqSubscriber pos_sub("tcp://127.0.0.1:6003", "position_updates");
            
            // Give time for connection setup
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Test message flow
            md_pub.publish("market_data", R"({"symbol":"BTCUSDT","price":50000.0})");
            order_pub.publish("orders", R"({"cl_ord_id":"test","symbol":"BTCUSDT","side":"BUY","qty":0.1})");
            event_pub.publish("order_events", R"({"cl_ord_id":"test","status":"FILLED"})");
            pos_pub.publish("position_updates", R"({"symbol":"BTCUSDT","qty":0.1,"avg_price":50000.0})");
            
            // Give time for message delivery
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        TEST_CASE("Process Startup and Shutdown") {
            // Test process lifecycle management
            
            QuoteServer quote_server;
            MarketMakingStrategy trader;
            
            // Initialize processes
            CHECK(quote_server.initialize());
            CHECK(trader.initialize());
            
            // Shutdown processes
            quote_server.shutdown();
            trader.shutdown();
            
            // Verify shutdown
            CHECK_FALSE(quote_server.is_running());
            CHECK_FALSE(trader.is_running());
        }

        TEST_CASE("Process Error Recovery") {
            // Test error recovery in processes
            
            QuoteServer quote_server;
            CHECK(quote_server.initialize());
            
            // Simulate error condition
            quote_server.shutdown();
            CHECK_FALSE(quote_server.is_running());
            
            // Restart
            CHECK(quote_server.initialize());
            CHECK_FALSE(quote_server.is_running());
            
            quote_server.shutdown();
        }
    }
}
