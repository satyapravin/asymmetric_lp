#include <doctest.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <chrono>

// Mock protocol buffer structures for testing
namespace proto {
    enum Side { BUY = 0, SELL = 1 };
    enum OrderType { MARKET = 0, LIMIT = 1, STOP = 2, STOP_LIMIT = 3 };
    enum EventType { ACK = 0, FILL = 1, CANCEL = 2, REJECT = 3 };
    
    struct OrderRequest {
        std::string cl_ord_id;
        std::string exch;
        std::string symbol;
        Side side;
        OrderType order_type;
        double qty;
        double price;
        uint64_t timestamp_us;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_order_request";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
    };
    
    struct OrderEvent {
        std::string cl_ord_id;
        std::string exch;
        std::string symbol;
        EventType event_type;
        double fill_qty;
        double fill_price;
        std::string text;
        uint64_t timestamp_us;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_order_event";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
    };
    
    struct MarketData {
        std::string symbol;
        std::string exch;
        double price;
        double qty;
        Side side;
        uint64_t timestamp_us;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_market_data";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
    };
    
    struct OrderbookLevel {
        double price;
        double qty;
    };
    
    struct Orderbook {
        std::string symbol;
        std::string exch;
        uint64_t timestamp_us;
        std::vector<OrderbookLevel> bids;
        std::vector<OrderbookLevel> asks;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_orderbook";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
        
        int bids_size() const { return bids.size(); }
        int asks_size() const { return asks.size(); }
        const OrderbookLevel& bids(int index) const { return bids[index]; }
        const OrderbookLevel& asks(int index) const { return asks[index]; }
        void add_bids() { bids.push_back({50000.0, 1.5}); }
        void add_asks() { asks.push_back({50001.0, 1.0}); }
    };
    
    struct Ticker {
        std::string symbol;
        std::string exch;
        double price;
        double volume;
        double change;
        double change_percent;
        uint64_t timestamp_us;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_ticker";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
    };
    
    struct Position {
        std::string symbol;
        double qty;
        double avg_price;
    };
    
    struct PositionUpdate {
        std::string exch;
        std::string symbol;
        double qty;
        double avg_price;
        uint64_t timestamp_us;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_position_update";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
    };
    
    struct PositionSnapshot {
        std::string exch;
        uint64_t timestamp_us;
        std::vector<Position> positions;
        
        bool SerializeToString(std::string* output) const {
            // Mock serialization
            *output = "mock_serialized_position_snapshot";
            return true;
        }
        
        bool ParseFromString(const std::string& input) {
            // Mock deserialization
            return !input.empty();
        }
        
        int positions_size() const { return positions.size(); }
        const Position& positions(int index) const { return positions[index]; }
        void add_positions() { positions.push_back({"BTCUSDT", 0.5, 50000.0}); }
    };
}

TEST_SUITE("Protocol Buffer Tests") {

    TEST_SUITE("Order Protocol Buffer Tests") {
        
        TEST_CASE("OrderRequest - Serialization and Deserialization") {
            // Create OrderRequest
            proto::OrderRequest request;
            request.cl_ord_id = "test_order_123";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            request.side = proto::BUY;
            request.order_type = proto::LIMIT;
            request.qty = 0.1;
            request.price = 50000.0;
            request.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Serialize to string
            std::string serialized;
            CHECK(request.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::OrderRequest deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.cl_ord_id, request.cl_ord_id);
            CHECK_EQ(deserialized.exch, request.exch);
            CHECK_EQ(deserialized.symbol, request.symbol);
            CHECK_EQ(deserialized.side, request.side);
            CHECK_EQ(deserialized.order_type, request.order_type);
            CHECK_EQ(deserialized.qty, request.qty);
            CHECK_EQ(deserialized.price, request.price);
            CHECK_EQ(deserialized.timestamp_us, request.timestamp_us);
        }

        TEST_CASE("OrderEvent - Serialization and Deserialization") {
            // Create OrderEvent
            proto::OrderEvent event;
            event.cl_ord_id = "test_order_123";
            event.exch = "BINANCE";
            event.symbol = "BTCUSDT";
            event.event_type = proto::FILL;
            event.fill_qty = 0.1;
            event.fill_price = 50000.0;
            event.text = "filled";
            event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Serialize to string
            std::string serialized;
            CHECK(event.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::OrderEvent deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.cl_ord_id, event.cl_ord_id);
            CHECK_EQ(deserialized.exch, event.exch);
            CHECK_EQ(deserialized.symbol, event.symbol);
            CHECK_EQ(deserialized.event_type, event.event_type);
            CHECK_EQ(deserialized.fill_qty, event.fill_qty);
            CHECK_EQ(deserialized.fill_price, event.fill_price);
            CHECK_EQ(deserialized.text, event.text);
            CHECK_EQ(deserialized.timestamp_us, event.timestamp_us);
        }

        TEST_CASE("OrderRequest - Field Validation") {
            proto::OrderRequest request;
            
            // Test required fields
            CHECK(request.cl_ord_id.empty());
            CHECK(request.exch.empty());
            CHECK(request.symbol.empty());
            
            // Set required fields
            request.cl_ord_id = "test_order";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            
            // Verify fields are set
            CHECK_FALSE(request.cl_ord_id.empty());
            CHECK_FALSE(request.exch.empty());
            CHECK_FALSE(request.symbol.empty());
        }

        TEST_CASE("OrderEvent - Event Types") {
            proto::OrderEvent event;
            event.cl_ord_id = "test_order";
            event.exch = "BINANCE";
            event.symbol = "BTCUSDT";
            
            // Test different event types
            event.event_type = proto::ACK;
            CHECK_EQ(event.event_type, proto::ACK);
            
            event.event_type = proto::FILL;
            CHECK_EQ(event.event_type, proto::FILL);
            
            event.event_type = proto::CANCEL;
            CHECK_EQ(event.event_type, proto::CANCEL);
            
            event.event_type = proto::REJECT;
            CHECK_EQ(event.event_type, proto::REJECT);
        }

        TEST_CASE("OrderRequest - Order Types") {
            proto::OrderRequest request;
            request.cl_ord_id = "test_order";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            
            // Test different order types
            request.order_type = proto::MARKET;
            CHECK_EQ(request.order_type, proto::MARKET);
            
            request.order_type = proto::LIMIT;
            CHECK_EQ(request.order_type, proto::LIMIT);
            
            request.order_type = proto::STOP;
            CHECK_EQ(request.order_type, proto::STOP);
            
            request.order_type = proto::STOP_LIMIT;
            CHECK_EQ(request.order_type, proto::STOP_LIMIT);
        }
    }

    TEST_SUITE("Market Data Protocol Buffer Tests") {
        
        TEST_CASE("MarketData - Serialization and Deserialization") {
            // Create MarketData
            proto::MarketData md;
            md.symbol = "BTCUSDT";
            md.exch = "BINANCE";
            md.price = 50000.0;
            md.qty = 0.1;
            md.side = proto::BUY;
            md.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Serialize to string
            std::string serialized;
            CHECK(md.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::MarketData deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol, md.symbol);
            CHECK_EQ(deserialized.exch, md.exch);
            CHECK_EQ(deserialized.price, md.price);
            CHECK_EQ(deserialized.qty, md.qty);
            CHECK_EQ(deserialized.side, md.side);
            CHECK_EQ(deserialized.timestamp_us, md.timestamp_us);
        }

        TEST_CASE("Orderbook - Serialization and Deserialization") {
            // Create Orderbook
            proto::Orderbook orderbook;
            orderbook.symbol = "BTCUSDT";
            orderbook.exch = "BINANCE";
            orderbook.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Add bids and asks
            orderbook.add_bids();
            orderbook.add_bids();
            orderbook.add_asks();
            orderbook.add_asks();
            
            // Serialize to string
            std::string serialized;
            CHECK(orderbook.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::Orderbook deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol, orderbook.symbol);
            CHECK_EQ(deserialized.exch, orderbook.exch);
            CHECK_EQ(deserialized.timestamp_us, orderbook.timestamp_us);
            CHECK_EQ(deserialized.bids_size(), 2);
            CHECK_EQ(deserialized.asks_size(), 2);
        }

        TEST_CASE("Ticker - Serialization and Deserialization") {
            // Create Ticker
            proto::Ticker ticker;
            ticker.symbol = "BTCUSDT";
            ticker.exch = "BINANCE";
            ticker.price = 50000.0;
            ticker.volume = 100.5;
            ticker.change = 1000.0;
            ticker.change_percent = 2.0;
            ticker.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Serialize to string
            std::string serialized;
            CHECK(ticker.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::Ticker deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.symbol, ticker.symbol);
            CHECK_EQ(deserialized.exch, ticker.exch);
            CHECK_EQ(deserialized.price, ticker.price);
            CHECK_EQ(deserialized.volume, ticker.volume);
            CHECK_EQ(deserialized.change, ticker.change);
            CHECK_EQ(deserialized.change_percent, ticker.change_percent);
            CHECK_EQ(deserialized.timestamp_us, ticker.timestamp_us);
        }
    }

    TEST_SUITE("Position Protocol Buffer Tests") {
        
        TEST_CASE("PositionUpdate - Serialization and Deserialization") {
            // Create PositionUpdate
            proto::PositionUpdate update;
            update.exch = "BINANCE";
            update.symbol = "BTCUSDT";
            update.qty = 0.5;
            update.avg_price = 50000.0;
            update.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Serialize to string
            std::string serialized;
            CHECK(update.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::PositionUpdate deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.exch, update.exch);
            CHECK_EQ(deserialized.symbol, update.symbol);
            CHECK_EQ(deserialized.qty, update.qty);
            CHECK_EQ(deserialized.avg_price, update.avg_price);
            CHECK_EQ(deserialized.timestamp_us, update.timestamp_us);
        }

        TEST_CASE("PositionSnapshot - Serialization and Deserialization") {
            // Create PositionSnapshot
            proto::PositionSnapshot snapshot;
            snapshot.exch = "BINANCE";
            snapshot.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Add positions
            snapshot.add_positions();
            snapshot.add_positions();
            
            // Serialize to string
            std::string serialized;
            CHECK(snapshot.SerializeToString(&serialized));
            CHECK_FALSE(serialized.empty());
            
            // Deserialize from string
            proto::PositionSnapshot deserialized;
            CHECK(deserialized.ParseFromString(serialized));
            
            // Verify data integrity
            CHECK_EQ(deserialized.exch, snapshot.exch);
            CHECK_EQ(deserialized.timestamp_us, snapshot.timestamp_us);
            CHECK_EQ(deserialized.positions_size(), 2);
        }
    }

    TEST_SUITE("Protocol Buffer Performance Tests") {
        
        TEST_CASE("Serialization Performance") {
            proto::OrderRequest request;
            request.cl_ord_id = "perf_test_order";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            request.side = proto::BUY;
            request.order_type = proto::LIMIT;
            request.qty = 0.1;
            request.price = 50000.0;
            request.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
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
            request.cl_ord_id = "perf_test_order";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            request.side = proto::BUY;
            request.order_type = proto::LIMIT;
            request.qty = 0.1;
            request.price = 50000.0;
            request.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
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
            request.cl_ord_id = "test_order_123";
            request.exch = "BINANCE";
            request.symbol = "BTCUSDT";
            request.side = proto::BUY;
            request.order_type = proto::LIMIT;
            request.qty = 0.1;
            request.price = 50000.0;
            request.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
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
            request.cl_ord_id = "test_order";
            // Don't set other required fields
            
            std::string partial_data;
            CHECK(request.SerializeToString(&partial_data));
            
            // Should still serialize successfully
            CHECK_FALSE(partial_data.empty());
        }
    }
}
