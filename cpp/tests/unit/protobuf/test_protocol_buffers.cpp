#include <doctest.h>
#include <iostream>
#include <string>
#include <chrono>
#include "../../../proto/order.pb.h"
#include "../../../proto/market_data.pb.h"
#include "../../../proto/position.pb.h"

TEST_SUITE("Protocol Buffer Tests") {

    TEST_SUITE("Order Protocol Buffer Tests") {
        
        TEST_CASE("OrderRequest - Serialization and Deserialization") {
            // Create OrderRequest
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_order_type(proto::LIMIT);
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
            CHECK_EQ(deserialized.order_type(), request.order_type());
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
            event.set_text("filled");
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
            
            // Test required fields
            CHECK_FALSE(request.has_cl_ord_id());
            CHECK_FALSE(request.has_exch());
            CHECK_FALSE(request.has_symbol());
            
            // Set required fields
            request.set_cl_ord_id("test_order");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            
            // Verify fields are set
            CHECK(request.has_cl_ord_id());
            CHECK(request.has_exch());
            CHECK(request.has_symbol());
        }

        TEST_CASE("OrderEvent - Event Types") {
            proto::OrderEvent event;
            event.set_cl_ord_id("test_order");
            event.set_exch("BINANCE");
            event.set_symbol("BTCUSDT");
            
            // Test different event types
            event.set_event_type(proto::ACK);
            CHECK_EQ(event.event_type(), proto::ACK);
            
            event.set_event_type(proto::FILL);
            CHECK_EQ(event.event_type(), proto::FILL);
            
            event.set_event_type(proto::CANCEL);
            CHECK_EQ(event.event_type(), proto::CANCEL);
            
            event.set_event_type(proto::REJECT);
            CHECK_EQ(event.event_type(), proto::REJECT);
        }

        TEST_CASE("OrderRequest - Order Types") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            
            // Test different order types
            request.set_order_type(proto::MARKET);
            CHECK_EQ(request.order_type(), proto::MARKET);
            
            request.set_order_type(proto::LIMIT);
            CHECK_EQ(request.order_type(), proto::LIMIT);
            
            request.set_order_type(proto::STOP);
            CHECK_EQ(request.order_type(), proto::STOP);
            
            request.set_order_type(proto::STOP_LIMIT);
            CHECK_EQ(request.order_type(), proto::STOP_LIMIT);
        }
    }

    TEST_SUITE("Market Data Protocol Buffer Tests") {
        
        TEST_CASE("MarketData - Serialization and Deserialization") {
            // Create MarketData
            proto::MarketData md;
            md.set_symbol("BTCUSDT");
            md.set_exch("BINANCE");
            md.set_price(50000.0);
            md.set_qty(0.1);
            md.set_side(proto::BUY);
            md.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(md.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::MarketData deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol(), md.symbol());
            CHECK_EQ(deserialized.exch(), md.exch());
            CHECK_EQ(deserialized.price(), md.price());
            CHECK_EQ(deserialized.qty(), md.qty());
            CHECK_EQ(deserialized.side(), md.side());
            CHECK_EQ(deserialized.timestamp_us(), md.timestamp_us());
        }

        TEST_CASE("Orderbook - Serialization and Deserialization") {
            // Create Orderbook
            proto::Orderbook orderbook;
            orderbook.set_symbol("BTCUSDT");
            orderbook.set_exch("BINANCE");
            orderbook.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Add bids
            auto* bid1 = orderbook.add_bids();
            bid1->set_price(50000.0);
            bid1->set_qty(1.5);
            
            auto* bid2 = orderbook.add_bids();
            bid2->set_price(49999.0);
            bid2->set_qty(2.0);
            
            // Add asks
            auto* ask1 = orderbook.add_asks();
            ask1->set_price(50001.0);
            ask1->set_qty(1.0);
            
            auto* ask2 = orderbook.add_asks();
            ask2->set_price(50002.0);
            ask2->set_qty(1.5);
            
            // Serialize to string
            std::string serialized;
            CHECK(orderbook.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::Orderbook deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol(), orderbook.symbol());
            CHECK_EQ(deserialized.exch(), orderbook.exch());
            CHECK_EQ(deserialized.timestamp_us(), orderbook.timestamp_us());
            CHECK_EQ(deserialized.bids_size(), 2);
            CHECK_EQ(deserialized.asks_size(), 2);
            
            // Verify bid data
            CHECK_EQ(deserialized.bids(0).price(), 50000.0);
            CHECK_EQ(deserialized.bids(0).qty(), 1.5);
            CHECK_EQ(deserialized.bids(1).price(), 49999.0);
            CHECK_EQ(deserialized.bids(1).qty(), 2.0);
            
            // Verify ask data
            CHECK_EQ(deserialized.asks(0).price(), 50001.0);
            CHECK_EQ(deserialized.asks(0).qty(), 1.0);
            CHECK_EQ(deserialized.asks(1).price(), 50002.0);
            CHECK_EQ(deserialized.asks(1).qty(), 1.5);
        }

        TEST_CASE("Ticker - Serialization and Deserialization") {
            // Create Ticker
            proto::Ticker ticker;
            ticker.set_symbol("BTCUSDT");
            ticker.set_exch("BINANCE");
            ticker.set_price(50000.0);
            ticker.set_volume(100.5);
            ticker.set_change(1000.0);
            ticker.set_change_percent(2.0);
            ticker.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Serialize to string
            std::string serialized;
            CHECK(ticker.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::Ticker deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol(), ticker.symbol());
            CHECK_EQ(deserialized.exch(), ticker.exch());
            CHECK_EQ(deserialized.price(), ticker.price());
            CHECK_EQ(deserialized.volume(), ticker.volume());
            CHECK_EQ(deserialized.change(), ticker.change());
            CHECK_EQ(deserialized.change_percent(), ticker.change_percent());
            CHECK_EQ(deserialized.timestamp_us(), ticker.timestamp_us());
        }
    }

    TEST_SUITE("Position Protocol Buffer Tests") {
        
        TEST_CASE("PositionUpdate - Serialization and Deserialization") {
            // Create PositionUpdate
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

        TEST_CASE("PositionSnapshot - Serialization and Deserialization") {
            // Create PositionSnapshot
            proto::PositionSnapshot snapshot;
            snapshot.set_exch("BINANCE");
            snapshot.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Add positions
            auto* pos1 = snapshot.add_positions();
            pos1->set_symbol("BTCUSDT");
            pos1->set_qty(0.5);
            pos1->set_avg_price(50000.0);
            
            auto* pos2 = snapshot.add_positions();
            pos2->set_symbol("ETHUSDT");
            pos2->set_qty(2.0);
            pos2->set_avg_price(3000.0);
            
            // Serialize to string
            std::string serialized;
            CHECK(snapshot.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::PositionSnapshot deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.exch(), snapshot.exch());
            CHECK_EQ(deserialized.timestamp_us(), snapshot.timestamp_us());
            CHECK_EQ(deserialized.positions_size(), 2);
            
            // Verify position data
            CHECK_EQ(deserialized.positions(0).symbol(), "BTCUSDT");
            CHECK_EQ(deserialized.positions(0).qty(), 0.5);
            CHECK_EQ(deserialized.positions(0).avg_price(), 50000.0);
            
            CHECK_EQ(deserialized.positions(1).symbol(), "ETHUSDT");
            CHECK_EQ(deserialized.positions(1).qty(), 2.0);
            CHECK_EQ(deserialized.positions(1).avg_price(), 3000.0);
        }
    }

    TEST_SUITE("Protocol Buffer Performance Tests") {
        
        TEST_CASE("Serialization Performance") {
            proto::OrderRequest request;
            request.set_cl_ord_id("perf_test_order");
            request.set_exch("BINANCE");
            request.set_symbol("BTCUSDT");
            request.set_side(proto::BUY);
            request.set_order_type(proto::LIMIT);
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
            
            // Should serialize quickly
            CHECK(duration.count() < 100000); // Less than 100ms for 10k iterations
        }

        TEST_CASE("Deserialization Performance") {
            proto::OrderRequest request;
            request.set_cl_ord_id("perf_test_order");
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
            
            const int iterations = 10000;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < iterations; ++i) {
                proto::OrderRequest deserialized;
                deserialized.ParseFromString(serialized);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // Should deserialize quickly
            CHECK(duration.count() < 100000); // Less than 100ms for 10k iterations
        }

        TEST_CASE("Message Size") {
            proto::OrderRequest request;
            request.set_cl_ord_id("test_order_123");
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
            
            // Message should be reasonably sized
            CHECK(serialized.size() < 1000); // Less than 1KB
            CHECK(serialized.size() > 50);   // More than 50 bytes
        }
    }

    TEST_SUITE("Protocol Buffer Error Handling") {
        
        TEST_CASE("Invalid Data Handling") {
            proto::OrderRequest request;
            
            // Try to parse invalid data
            std::string invalid_data = "invalid_protobuf_data";
            CHECK_FALSE(request.ParseFromString(invalid_data));
        }

        TEST_CASE("Empty Message Handling") {
            proto::OrderRequest request;
            
            // Try to parse empty data
            std::string empty_data = "";
            CHECK_FALSE(request.ParseFromString(empty_data));
        }

        TEST_CASE("Partial Data Handling") {
            proto::OrderRequest request;
            
            // Create partial message
            request.set_cl_ord_id("test_order");
            // Don't set other required fields
            
            std::string partial_data;
            CHECK(request.SerializeToString(&partial_data));
            
            // Should still serialize successfully
            CHECK_FALSE(partial_data.empty());
        }
    }
}
