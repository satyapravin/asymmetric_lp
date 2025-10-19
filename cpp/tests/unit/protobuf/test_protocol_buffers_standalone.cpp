#include <doctest.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include "proto/order.pb.h"
#include "proto/market_data.pb.h"
#include "proto/position.pb.h"

TEST_SUITE("Protocol Buffer Tests") {

    TEST_SUITE("Order Protocol Buffer Tests") {
        
        TEST_CASE("OrderRequest - Serialization and Deserialization") {
            // Create OrderRequest
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_type(proto::LIMIT);
            request.set_qty(0.1);
            request.set_price(50000.0);
            request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(request.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::OrderRequest deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.cl_ord_id(), request.cl_ord_id());
            CHECK_EQ(deserialized.exch(), request.exch());
            CHECK_EQ(deserialized.symbol(), request.symbol());
            CHECK_EQ(deserialized.side(), request.side());
            CHECK_EQ(deserialized.type(), request.type());
            CHECK_EQ(deserialized.qty(), request.qty());
            CHECK_EQ(deserialized.price(), request.price());
            CHECK_EQ(deserialized.timestamp_us(), request.timestamp_us());
        }
        
        TEST_CASE("OrderEvent - Serialization and Deserialization") {
            // Create OrderEvent
            proto::OrderEvent event;
            event.set_cl_ord_id("test_order_123");
            event.set_exch("BINANCE");
            event.set_symbol("BTCUSDT");
            event.set_event_type(proto::FILL);
            event.set_fill_qty(0.1);
            event.set_fill_price(50000.0);
            event.set_text("Filled");
            event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(event.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::OrderEvent deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.cl_ord_id(), event.cl_ord_id());
            CHECK_EQ(deserialized.exch(), event.exch());
            CHECK_EQ(deserialized.symbol(), event.symbol());
            CHECK_EQ(deserialized.event_type(), event.event_type());
            CHECK_EQ(deserialized.fill_qty(), event.fill_qty());
            CHECK_EQ(deserialized.fill_price(), event.fill_price());
            CHECK_EQ(deserialized.text(), event.text());
            CHECK_EQ(deserialized.timestamp_us(), event.timestamp_us());
        }
        
        TEST_CASE("OrderRequest - Field Validation") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            
            // Test different order types
            request.set_type(proto::MARKET);
            CHECK_EQ(request.type(), proto::MARKET);
            
            request.set_type(proto::LIMIT);
            CHECK_EQ(request.type(), proto::LIMIT);
        }
        
        TEST_CASE("OrderRequest - Order Types") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            
            // Test different order types
            request.set_type(proto::MARKET);
            CHECK_EQ(request.type(), proto::MARKET);
            
            request.set_type(proto::LIMIT);
            CHECK_EQ(request.type(), proto::LIMIT);
        }
    }

    TEST_SUITE("Market Data Protocol Buffer Tests") {
        
        TEST_CASE("Trade - Serialization and Deserialization") {
            proto::Trade trade;
            trade.set_exch("BINANCE");
            trade.set_symbol("BTCUSDT");
            trade.set_price(50000.0);
            trade.set_qty(0.1);
            trade.set_is_buyer_maker(false); // false means buyer is taker (buy)
            trade.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(trade.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::Trade deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol(), trade.symbol());
            CHECK_EQ(deserialized.exch(), trade.exch());
            CHECK_EQ(deserialized.price(), trade.price());
            CHECK_EQ(deserialized.qty(), trade.qty());
            CHECK_EQ(deserialized.is_buyer_maker(), trade.is_buyer_maker());
            CHECK_EQ(deserialized.timestamp_us(), trade.timestamp_us());
        }
        
        TEST_CASE("OrderBookSnapshot - Serialization and Deserialization") {
            proto::OrderBookSnapshot orderbook;
            orderbook.set_symbol("BTCUSDT");
            orderbook.set_exch("BINANCE");
            orderbook.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Add bid levels
            auto* bid = orderbook.add_bids();
            bid->set_price(50000.0);
            bid->set_qty(1.0);
            
            auto* ask = orderbook.add_asks();
            ask->set_price(50001.0);
            ask->set_qty(1.5);
            
            // Serialize to string
            std::string serialized;
            CHECK(orderbook.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::OrderBookSnapshot deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol(), orderbook.symbol());
            CHECK_EQ(deserialized.exch(), orderbook.exch());
            CHECK_EQ(deserialized.timestamp_us(), orderbook.timestamp_us());
            CHECK_EQ(deserialized.bids_size(), 1);
            CHECK_EQ(deserialized.asks_size(), 1);
            CHECK_EQ(deserialized.bids(0).price(), 50000.0);
            CHECK_EQ(deserialized.bids(0).qty(), 1.0);
            CHECK_EQ(deserialized.asks(0).price(), 50001.0);
            CHECK_EQ(deserialized.asks(0).qty(), 1.5);
        }
        
        TEST_CASE("PositionUpdate - Serialization and Deserialization") {
            proto::PositionUpdate update;
            update.set_exch("BINANCE");
            update.set_symbol("BTCUSDT");
            update.set_qty(0.5);
            update.set_avg_price(50000.0);
            update.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(update.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::PositionUpdate deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.exch(), update.exch());
            CHECK_EQ(deserialized.symbol(), update.symbol());
            CHECK_EQ(deserialized.qty(), update.qty());
            CHECK_EQ(deserialized.avg_price(), update.avg_price());
            CHECK_EQ(deserialized.timestamp_us(), update.timestamp_us());
        }
    }

    TEST_SUITE("Position Protocol Buffer Tests") {
        
        TEST_CASE("PositionUpdate - Serialization and Deserialization") {
            proto::PositionUpdate update;
            update.set_exch("BINANCE");
            update.set_symbol("BTCUSDT");
            update.set_qty(0.5);
            update.set_avg_price(50000.0);
            update.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(update.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::PositionUpdate deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.exch(), update.exch());
            CHECK_EQ(deserialized.symbol(), update.symbol());
            CHECK_EQ(deserialized.qty(), update.qty());
            CHECK_EQ(deserialized.avg_price(), update.avg_price());
            CHECK_EQ(deserialized.timestamp_us(), update.timestamp_us());
        }
    }

    TEST_SUITE("Performance Tests") {
        
        TEST_CASE("Serialization Performance") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_type(proto::LIMIT);
            request.set_qty(0.1);
            request.set_price(50000.0);
            request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            const int iterations = 10000;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < iterations; ++i) {
                std::string serialized;
                request.SerializeToString(&serialized);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // Should serialize 10,000 messages in less than 100ms
            CHECK(duration.count() < 100000);
        }
        
        TEST_CASE("Deserialization Performance") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_type(proto::LIMIT);
            request.set_qty(0.1);
            request.set_price(50000.0);
            request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            std::string serialized;
            request.SerializeToString(&serialized);
            
            const int iterations = 10000;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < iterations; ++i) {
                proto::OrderRequest deserialized;
                deserialized.ParseFromString(serialized);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // Should deserialize 10,000 messages in less than 100ms
            CHECK(duration.count() < 100000);
        }
        
        TEST_CASE("Message Size") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_type(proto::LIMIT);
            request.set_qty(0.1);
            request.set_price(50000.0);
            request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            std::string serialized;
            request.SerializeToString(&serialized);
            
            // Message should be reasonably small (less than 1KB)
            CHECK(serialized.size() < 1024);
        }
    }

    TEST_SUITE("Error Handling Tests") {
        
        TEST_CASE("Invalid Data Handling") {
            proto::OrderRequest request;
            
            // Test with invalid data
            std::string invalid_data = "invalid_protobuf_data";
            CHECK_FALSE(request.ParseFromString(invalid_data));
        }
        
        TEST_CASE("Empty Message Handling") {
            proto::OrderRequest request;
            
            // Test with empty data - should parse successfully (empty is valid)
            std::string empty_data = "";
            CHECK(request.ParseFromString(empty_data));
        }
        
        TEST_CASE("Partial Data Handling") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order");
            // Don't set other required fields
            
            std::string serialized;
            CHECK(request.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Should be able to deserialize even with partial data
            proto::OrderRequest deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            CHECK_EQ(deserialized.cl_ord_id(), "test_order");
        }
    }
}
