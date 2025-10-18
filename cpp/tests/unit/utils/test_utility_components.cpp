#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../../../utils/http/curl_http_handler.hpp"
#include "../../../utils/http/i_http_handler.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include "../../../utils/zmq/zmq_subscriber.hpp"
#include "../../../utils/mds/market_data_normalizer.hpp"
#include "../../../utils/mds/orderbook_binary.hpp"
#include "../../../utils/oms/order.hpp"
#include "../../../utils/oms/order_binary.hpp"
#include "../../../utils/oms/order_manager.hpp"
#include "../../../utils/logging/logger.hpp"

TEST_SUITE("Utility Component Tests") {

    TEST_SUITE("HTTP Handler Tests") {
        
        TEST_CASE("Curl HTTP Handler - Constructor and Destructor") {
            CurlHttpHandler handler;
            CHECK_FALSE(handler.is_connected());
        }

        TEST_CASE("Curl HTTP Handler - Connect and Disconnect") {
            CurlHttpHandler handler;
            
            // Test connection to a valid URL
            CHECK(handler.connect("https://httpbin.org"));
            CHECK(handler.is_connected());
            
            handler.disconnect();
            CHECK_FALSE(handler.is_connected());
        }

        TEST_CASE("Curl HTTP Handler - GET Request") {
            CurlHttpHandler handler;
            
            CHECK(handler.connect("https://httpbin.org"));
            
            // Test GET request
            auto response = handler.get("/get");
            CHECK(response.has_value());
            
            handler.disconnect();
        }

        TEST_CASE("Curl HTTP Handler - POST Request") {
            CurlHttpHandler handler;
            
            CHECK(handler.connect("https://httpbin.org"));
            
            // Test POST request
            std::string post_data = R"({"test": "data"})";
            auto response = handler.post("/post", post_data, "application/json");
            CHECK(response.has_value());
            
            handler.disconnect();
        }

        TEST_CASE("Curl HTTP Handler - Error Handling") {
            CurlHttpHandler handler;
            
            // Test connection to invalid URL
            CHECK_FALSE(handler.connect("https://invalid-url-that-does-not-exist.com"));
            
            // Test request without connection
            auto response = handler.get("/test");
            CHECK_FALSE(response.has_value());
        }

        TEST_CASE("Curl HTTP Handler - Timeout") {
            CurlHttpHandler handler;
            
            CHECK(handler.connect("https://httpbin.org"));
            
            // Test with timeout
            handler.set_timeout(1); // 1 second timeout
            
            // This should succeed
            auto response = handler.get("/get");
            CHECK(response.has_value());
            
            handler.disconnect();
        }
    }

    TEST_SUITE("ZMQ Publisher/Subscriber Tests") {
        
        TEST_CASE("ZMQ Publisher - Constructor and Destructor") {
            ZmqPublisher publisher("tcp://127.0.0.1:5555");
            // Should not crash
        }

        TEST_CASE("ZMQ Subscriber - Constructor and Destructor") {
            ZmqSubscriber subscriber("tcp://127.0.0.1:5555", "test_topic");
            // Should not crash
        }

        TEST_CASE("ZMQ Publisher/Subscriber - Basic Communication") {
            ZmqPublisher publisher("tcp://127.0.0.1:5556");
            ZmqSubscriber subscriber("tcp://127.0.0.1:5556", "test_topic");
            
            // Give time for connection setup
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Publish message
            std::string test_message = "Hello, ZMQ!";
            publisher.publish("test_topic", test_message);
            
            // Give time for message delivery
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Receive message
            auto received = subscriber.receive();
            CHECK(received.has_value());
            CHECK_EQ(*received, test_message);
        }

        TEST_CASE("ZMQ Publisher/Subscriber - Multiple Messages") {
            ZmqPublisher publisher("tcp://127.0.0.1:5557");
            ZmqSubscriber subscriber("tcp://127.0.0.1:5557", "multi_topic");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Send multiple messages
            for (int i = 0; i < 5; ++i) {
                std::string message = "Message " + std::to_string(i);
                publisher.publish("multi_topic", message);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Receive messages
            int received_count = 0;
            while (received_count < 5) {
                auto received = subscriber.receive();
                if (received.has_value()) {
                    received_count++;
                } else {
                    break;
                }
            }
            
            CHECK_EQ(received_count, 5);
        }

        TEST_CASE("ZMQ Publisher/Subscriber - Different Topics") {
            ZmqPublisher publisher("tcp://127.0.0.1:5558");
            ZmqSubscriber subscriber1("tcp://127.0.0.1:5558", "topic1");
            ZmqSubscriber subscriber2("tcp://127.0.0.1:5558", "topic2");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Publish to different topics
            publisher.publish("topic1", "Message for topic1");
            publisher.publish("topic2", "Message for topic2");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Each subscriber should only receive their topic
            auto msg1 = subscriber1.receive();
            auto msg2 = subscriber2.receive();
            
            CHECK(msg1.has_value());
            CHECK(msg2.has_value());
            CHECK_EQ(*msg1, "Message for topic1");
            CHECK_EQ(*msg2, "Message for topic2");
        }
    }

    TEST_SUITE("Market Data System Tests") {
        
        TEST_CASE("Market Data Normalizer - Basic Functionality") {
            MarketDataNormalizer normalizer;
            
            // Test with sample market data
            std::string raw_data = R"({"symbol":"BTCUSDT","price":"50000.00","qty":"0.1"})";
            
            auto normalized = normalizer.normalize(raw_data, "BINANCE");
            CHECK(normalized.has_value());
        }

        TEST_CASE("Orderbook Binary - Serialization") {
            OrderbookBinary orderbook;
            
            // Add some bids and asks
            orderbook.add_bid(50000.0, 1.5);
            orderbook.add_bid(49999.0, 2.0);
            orderbook.add_ask(50001.0, 1.0);
            orderbook.add_ask(50002.0, 1.5);
            
            // Serialize to binary
            auto binary_data = orderbook.serialize();
            CHECK_FALSE(binary_data.empty());
            
            // Deserialize back
            OrderbookBinary deserialized;
            CHECK(deserialized.deserialize(binary_data));
            
            // Verify data integrity
            CHECK_EQ(deserialized.get_bid_count(), 2);
            CHECK_EQ(deserialized.get_ask_count(), 2);
        }

        TEST_CASE("Orderbook Binary - Data Integrity") {
            OrderbookBinary orderbook;
            
            // Add test data
            orderbook.add_bid(50000.0, 1.5);
            orderbook.add_ask(50001.0, 1.0);
            
            // Serialize and deserialize
            auto binary_data = orderbook.serialize();
            OrderbookBinary restored;
            CHECK(restored.deserialize(binary_data));
            
            // Check data integrity
            auto bids = restored.get_bids();
            auto asks = restored.get_asks();
            
            CHECK_EQ(bids.size(), 1);
            CHECK_EQ(asks.size(), 1);
            CHECK_EQ(bids[0].first, 50000.0);
            CHECK_EQ(bids[0].second, 1.5);
            CHECK_EQ(asks[0].first, 50001.0);
            CHECK_EQ(asks[0].second, 1.0);
        }
    }

    TEST_SUITE("Order Management System Tests") {
        
        TEST_CASE("Order Binary - Serialization") {
            Order order;
            order.cl_ord_id = "test_order_123";
            order.symbol = "BTCUSDT";
            order.side = OrderSide::BUY;
            order.order_type = OrderType::LIMIT;
            order.qty = 0.1;
            order.price = 50000.0;
            
            // Serialize order
            char buffer[OrderBinaryHelper::ORDER_SIZE];
            CHECK(OrderBinaryHelper::serialize_order(order.cl_ord_id, order.symbol, 
                                                   static_cast<uint32_t>(order.side), 
                                                   static_cast<uint32_t>(order.order_type),
                                                   order.qty, order.price, buffer));
            
            // Deserialize order
            std::string cl_ord_id, symbol;
            uint32_t side, order_type;
            double qty, price;
            
            CHECK(OrderBinaryHelper::deserialize_order(buffer, cl_ord_id, symbol, 
                                                     side, order_type, qty, price));
            
            // Verify data integrity
            CHECK_EQ(cl_ord_id, order.cl_ord_id);
            CHECK_EQ(symbol, order.symbol);
            CHECK_EQ(side, static_cast<uint32_t>(order.side));
            CHECK_EQ(order_type, static_cast<uint32_t>(order.order_type));
            CHECK_EQ(qty, order.qty);
            CHECK_EQ(price, order.price);
        }

        TEST_CASE("Order Manager - Order Lifecycle") {
            OrderManager order_manager;
            
            // Create order
            Order order;
            order.cl_ord_id = "lifecycle_test_123";
            order.symbol = "BTCUSDT";
            order.side = OrderSide::BUY;
            order.order_type = OrderType::LIMIT;
            order.qty = 0.1;
            order.price = 50000.0;
            
            // Add order
            CHECK(order_manager.add_order(order));
            
            // Get order
            auto retrieved_order = order_manager.get_order(order.cl_ord_id);
            CHECK(retrieved_order.has_value());
            CHECK_EQ(retrieved_order->cl_ord_id, order.cl_ord_id);
            
            // Update order status
            CHECK(order_manager.update_order_status(order.cl_ord_id, OrderStatus::FILLED));
            
            // Verify status update
            auto updated_order = order_manager.get_order(order.cl_ord_id);
            CHECK(updated_order.has_value());
            CHECK_EQ(updated_order->status, OrderStatus::FILLED);
            
            // Remove order
            CHECK(order_manager.remove_order(order.cl_ord_id));
            
            // Verify removal
            auto removed_order = order_manager.get_order(order.cl_ord_id);
            CHECK_FALSE(removed_order.has_value());
        }

        TEST_CASE("Order Manager - Concurrent Operations") {
            OrderManager order_manager;
            
            // Test concurrent order operations
            std::vector<std::thread> threads;
            std::atomic<int> success_count{0};
            
            for (int i = 0; i < 10; ++i) {
                threads.emplace_back([&, i]() {
                    Order order;
                    order.cl_ord_id = "concurrent_" + std::to_string(i);
                    order.symbol = "BTCUSDT";
                    order.side = OrderSide::BUY;
                    order.order_type = OrderType::LIMIT;
                    order.qty = 0.1;
                    order.price = 50000.0 + i;
                    
                    if (order_manager.add_order(order)) {
                        success_count++;
                        
                        // Update status
                        order_manager.update_order_status(order.cl_ord_id, OrderStatus::FILLED);
                        
                        // Remove order
                        order_manager.remove_order(order.cl_ord_id);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            CHECK_EQ(success_count.load(), 10);
        }
    }

    TEST_SUITE("Logger Tests") {
        
        TEST_CASE("Logger - Basic Functionality") {
            Logger logger("test_logger");
            
            // Test different log levels
            logger.debug("Debug message");
            logger.info("Info message");
            logger.warn("Warning message");
            logger.error("Error message");
            
            // Should not crash
        }

        TEST_CASE("Logger - File Output") {
            Logger logger("file_logger", "test.log");
            
            logger.info("Test message to file");
            
            // Check if file was created
            std::ifstream file("test.log");
            CHECK(file.is_open());
            
            // Clean up
            file.close();
            std::remove("test.log");
        }

        TEST_CASE("Logger - Thread Safety") {
            Logger logger("thread_safe_logger");
            
            // Test concurrent logging
            std::vector<std::thread> threads;
            
            for (int i = 0; i < 5; ++i) {
                threads.emplace_back([&, i]() {
                    for (int j = 0; j < 10; ++j) {
                        logger.info("Thread " + std::to_string(i) + " message " + std::to_string(j));
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            // Should not crash with concurrent access
        }
    }
}
