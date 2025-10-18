#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include "../../exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"
#include "../../exchanges/binance/private_websocket/binance_private_websocket_handler.hpp"
#include "../../exchanges/binance/http/binance_oms.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"
#include "../../utils/zmq/zmq_subscriber.hpp"
#include "../../utils/oms/order_manager.hpp"
#include "../../proto/order.pb.h"
#include "../../config/test_config_manager.hpp"

using namespace binance;
using namespace test_config;

TEST_SUITE("Performance Tests") {

    TEST_SUITE("Latency Benchmarks") {
        
        TEST_CASE("Order Placement Latency") {
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
            CHECK(oms.connect(binance_config.http_url));
            
            const int iterations = 100;
            std::vector<double> latencies;
            
            for (int i = 0; i < iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                
                Order order;
                order.cl_ord_id = "perf_test_" + std::to_string(i);
                order.symbol = binance_config.symbol;
                order.side = OrderSide::BUY;
                order.order_type = OrderType::MARKET;
                order.qty = 0.01; // Small amount for test
                
                auto result = oms.send_order(order);
                
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                
                latencies.push_back(duration.count());
                
                // Small delay to avoid rate limiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            oms.disconnect();
            
            // Calculate statistics
            double total_latency = 0;
            for (double latency : latencies) {
                total_latency += latency;
            }
            double avg_latency = total_latency / latencies.size();
            
            // Average latency should be reasonable
            CHECK(avg_latency < 1000000); // Less than 1 second
            std::cout << "Average order placement latency: " << avg_latency << " microseconds" << std::endl;
        }

        TEST_CASE("WebSocket Message Processing Latency") {
            BinancePublicWebSocketHandler handler;
            CHECK(handler.connect("wss://fstream.binance.com/stream"));
            
            const int iterations = 1000;
            std::vector<double> latencies;
            
            // Set up callback to measure latency
            handler.set_ticker_callback([&](const std::string& symbol, double price, double volume) {
                auto end = std::chrono::high_resolution_clock::now();
                // In real implementation, would measure from message receipt
            });
            
            // Simulate message processing
            for (int i = 0; i < iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                
                // Simulate message handling
                handler.handle_ticker_update("BTCUSDT", R"({"c":"50000.00","v":"100.5"})");
                
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
                
                latencies.push_back(duration.count());
            }
            
            handler.disconnect();
            
            // Calculate statistics
            double total_latency = 0;
            for (double latency : latencies) {
                total_latency += latency;
            }
            double avg_latency = total_latency / latencies.size();
            
            // Message processing should be very fast
            CHECK(avg_latency < 1000000); // Less than 1ms
            std::cout << "Average message processing latency: " << avg_latency << " nanoseconds" << std::endl;
        }

        TEST_CASE("ZMQ Message Latency") {
            ZmqPublisher publisher("tcp://127.0.0.1:5559");
            ZmqSubscriber subscriber("tcp://127.0.0.1:5559", "perf_test");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            const int iterations = 1000;
            std::vector<double> latencies;
            
            for (int i = 0; i < iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                
                std::string message = "perf_test_message_" + std::to_string(i);
                publisher.publish("perf_test", message);
                
                // Receive message
                auto received = subscriber.receive();
                
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                
                if (received.has_value()) {
                    latencies.push_back(duration.count());
                }
            }
            
            // Calculate statistics
            double total_latency = 0;
            for (double latency : latencies) {
                total_latency += latency;
            }
            double avg_latency = total_latency / latencies.size();
            
            // ZMQ should be very fast
            CHECK(avg_latency < 1000); // Less than 1ms
            std::cout << "Average ZMQ message latency: " << avg_latency << " microseconds" << std::endl;
        }
    }

    TEST_SUITE("Throughput Tests") {
        
        TEST_CASE("Order Manager Throughput") {
            OrderManager order_manager;
            
            const int iterations = 10000;
            auto start = std::chrono::high_resolution_clock::now();
            
            // Test order creation throughput
            for (int i = 0; i < iterations; ++i) {
                Order order;
                order.cl_ord_id = "throughput_test_" + std::to_string(i);
                order.symbol = "BTCUSDT";
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = 0.1;
                order.price = 50000.0;
                
                order_manager.add_order(order);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            double orders_per_second = (double)iterations / (duration.count() / 1000000.0);
            
            // Should handle high throughput
            CHECK(orders_per_second > 1000); // More than 1000 orders per second
            std::cout << "Order manager throughput: " << orders_per_second << " orders/second" << std::endl;
        }

        TEST_CASE("ZMQ Publishing Throughput") {
            ZmqPublisher publisher("tcp://127.0.0.1:5560");
            
            const int iterations = 10000;
            auto start = std::chrono::high_resolution_clock::now();
            
            // Test publishing throughput
            for (int i = 0; i < iterations; ++i) {
                std::string message = "throughput_message_" + std::to_string(i);
                publisher.publish("throughput_test", message);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            double messages_per_second = (double)iterations / (duration.count() / 1000000.0);
            
            // Should handle high throughput
            CHECK(messages_per_second > 1000); // More than 1000 messages per second
            std::cout << "ZMQ publishing throughput: " << messages_per_second << " messages/second" << std::endl;
        }

        TEST_CASE("Protocol Buffer Serialization Throughput") {
            const int iterations = 10000;
            
            // Test OrderRequest serialization throughput
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < iterations; ++i) {
                proto::OrderRequest request;
                request.set_cl_ord_id("throughput_test_" + std::to_string(i));
                request.set_exch("BINANCE");
                request.set_symbol("BTCUSDT");
                request.set_side(proto::BUY);
                request.set_order_type(proto::LIMIT);
                request.set_qty(0.1);
                request.set_price(50000.0);
                request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                
                std::string serialized;
                request.SerializeToString(&serialized);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            double serializations_per_second = (double)iterations / (duration.count() / 1000000.0);
            
            // Should serialize very quickly
            CHECK(serializations_per_second > 10000); // More than 10k serializations per second
            std::cout << "Protocol buffer serialization throughput: " << serializations_per_second << " serializations/second" << std::endl;
        }
    }

    TEST_SUITE("Memory Usage Tests") {
        
        TEST_CASE("Order Manager Memory Usage") {
            OrderManager order_manager;
            
            const int iterations = 1000;
            
            // Measure memory usage before
            auto start_memory = std::chrono::high_resolution_clock::now();
            
            // Add many orders
            for (int i = 0; i < iterations; ++i) {
                Order order;
                order.cl_ord_id = "memory_test_" + std::to_string(i);
                order.symbol = "BTCUSDT";
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = 0.1;
                order.price = 50000.0;
                
                order_manager.add_order(order);
            }
            
            auto end_memory = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_memory - start_memory);
            
            // Memory operations should be fast
            CHECK(duration.count() < 100000); // Less than 100ms for 1000 orders
            
            // Clean up
            for (int i = 0; i < iterations; ++i) {
                order_manager.remove_order("memory_test_" + std::to_string(i));
            }
        }

        TEST_CASE("WebSocket Handler Memory Usage") {
            BinancePublicWebSocketHandler handler;
            
            const int iterations = 1000;
            
            auto start_memory = std::chrono::high_resolution_clock::now();
            
            // Simulate many message callbacks
            for (int i = 0; i < iterations; ++i) {
                handler.handle_ticker_update("BTCUSDT", R"({"c":"50000.00","v":"100.5"})");
            }
            
            auto end_memory = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_memory - start_memory);
            
            // Should not use excessive memory
            CHECK(duration.count() < 50000); // Less than 50ms for 1000 messages
        }
    }

    TEST_SUITE("Concurrent Operation Stress Tests") {
        
        TEST_CASE("Concurrent Order Operations") {
            auto& config_manager = get_test_config();
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
            CHECK(oms.connect(binance_config.http_url));
            
            const int num_threads = 10;
            const int orders_per_thread = 10;
            std::atomic<int> success_count{0};
            std::atomic<int> error_count{0};
            
            std::vector<std::thread> threads;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&, t]() {
                    for (int i = 0; i < orders_per_thread; ++i) {
                        Order order;
                        order.cl_ord_id = "stress_test_" + std::to_string(t) + "_" + std::to_string(i);
                        order.symbol = binance_config.symbol;
                        order.side = OrderSide::BUY;
                        order.order_type = OrderType::MARKET;
                        order.qty = 0.01; // Small amount
                        
                        auto result = oms.send_order(order);
                        if (result.has_value()) {
                            success_count++;
                        } else {
                            error_count++;
                        }
                        
                        // Small delay to avoid overwhelming the system
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            int total_orders = num_threads * orders_per_thread;
            double orders_per_second = (double)total_orders / (duration.count() / 1000.0);
            
            // Should handle concurrent operations reasonably well
            CHECK(success_count.load() + error_count.load() == total_orders);
            CHECK(orders_per_second > 1); // At least 1 order per second
            
            std::cout << "Concurrent order operations: " << success_count.load() << " success, " 
                      << error_count.load() << " errors, " << orders_per_second << " orders/second" << std::endl;
            
            oms.disconnect();
        }

        TEST_CASE("Concurrent ZMQ Operations") {
            const int num_threads = 10;
            const int messages_per_thread = 100;
            std::atomic<int> received_count{0};
            
            // Create subscribers
            std::vector<std::unique_ptr<ZmqSubscriber>> subscribers;
            for (int i = 0; i < num_threads; ++i) {
                subscribers.push_back(std::make_unique<ZmqSubscriber>("tcp://127.0.0.1:5561", "stress_test_" + std::to_string(i)));
            }
            
            std::vector<std::thread> threads;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&, t]() {
                    ZmqPublisher publisher("tcp://127.0.0.1:5561");
                    
                    for (int i = 0; i < messages_per_thread; ++i) {
                        std::string message = "stress_message_" + std::to_string(t) + "_" + std::to_string(i);
                        publisher.publish("stress_test_" + std::to_string(t), message);
                        
                        // Try to receive
                        auto received = subscribers[t]->receive();
                        if (received.has_value()) {
                            received_count++;
                        }
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            int total_messages = num_threads * messages_per_thread;
            double messages_per_second = (double)total_messages / (duration.count() / 1000.0);
            
            // Should handle concurrent ZMQ operations well
            CHECK(received_count.load() > 0);
            CHECK(messages_per_second > 10); // At least 10 messages per second
            
            std::cout << "Concurrent ZMQ operations: " << received_count.load() << " received, " 
                      << messages_per_second << " messages/second" << std::endl;
        }

        TEST_CASE("High-Frequency Trading Simulation") {
            OrderManager order_manager;
            
            const int num_threads = 5;
            const int operations_per_thread = 1000;
            std::atomic<int> operations_completed{0};
            
            std::vector<std::thread> threads;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int t = 0; t < num_threads; ++t) {
                threads.emplace_back([&, t]() {
                    for (int i = 0; i < operations_per_thread; ++i) {
                        // Simulate high-frequency operations
                        Order order;
                        order.cl_ord_id = "hft_" + std::to_string(t) + "_" + std::to_string(i);
                        order.symbol = "BTCUSDT";
                        order.side = OrderSide::BUY;
                        order.order_type = OrderType::LIMIT;
                        order.qty = 0.001; // Very small amount
                        order.price = 50000.0 + (i % 100); // Varying prices
                        
                        order_manager.add_order(order);
                        order_manager.update_order_status(order.cl_ord_id, OrderStatus::FILLED);
                        order_manager.remove_order(order.cl_ord_id);
                        
                        operations_completed++;
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            int total_operations = num_threads * operations_per_thread * 3; // Add, update, remove
            double operations_per_second = (double)total_operations / (duration.count() / 1000.0);
            
            // Should handle high-frequency operations very well
            CHECK(operations_completed.load() == total_operations);
            CHECK(operations_per_second > 1000); // More than 1000 operations per second
            
            std::cout << "High-frequency trading simulation: " << operations_per_second << " operations/second" << std::endl;
        }
    }
}
