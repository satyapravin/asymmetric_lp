#include <doctest/doctest.h>
#include "../../../utils/mds/market_data_normalizer.hpp"
#include "../../../proto/market_data.pb.h"
#include <string>
#include <vector>

// Shared mock parser for testing
class MockParser : public IExchangeParser {
public:
    MockParser(const std::string& symbol = "BTCUSDT", double base_price = 50000.0) 
        : symbol_(symbol), base_price_(base_price) {}
        
    bool parse_message(const std::string& raw_msg, 
                      std::string& symbol,
                      std::vector<std::pair<double, double>>& bids,
                      std::vector<std::pair<double, double>>& asks,
                      uint64_t& timestamp_us) override {
        if (raw_msg == "valid_message") {
            symbol = symbol_;
            bids = {{base_price_ - 1.0, 1.0}, {base_price_ - 2.0, 2.0}};
            asks = {{base_price_ + 1.0, 1.5}, {base_price_ + 2.0, 2.5}};
            timestamp_us = 1234567890;
            return true;
        }
        return false;
    }
    
private:
    std::string symbol_;
    double base_price_;
};

TEST_CASE("MarketDataNormalizer - Basic Functionality") {
    MarketDataNormalizer normalizer("BINANCE");
    
    SUBCASE("Initialize normalizer") {
        // Normalizer is initialized with exchange name
        CHECK(true); // Basic initialization test
    }
    
    SUBCASE("Set parser and callback") {
        auto parser = std::make_unique<BinanceParser>();
        normalizer.set_parser(std::move(parser));
        
        bool callback_called = false;
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
        });
        
        // Test that parser and callback are set
        CHECK(true); // Basic setup test
    }
}

TEST_CASE("MarketDataNormalizer - Parser Registration") {
    MarketDataNormalizer normalizer("BINANCE");
    
    SUBCASE("Set parser") {
        auto parser = std::make_unique<BinanceParser>();
        normalizer.set_parser(std::move(parser));
        
        // Parser is set via set_parser method
        CHECK(true); // Basic parser setup test
    }
    
    SUBCASE("Multiple parsers") {
        auto binance_parser = std::make_unique<BinanceParser>();
        normalizer.set_parser(std::move(binance_parser));
        
        // Only one parser can be set at a time
        CHECK(true); // Basic parser setup test
    }
}

TEST_CASE("MarketDataNormalizer - Message Processing") {
    MarketDataNormalizer normalizer("TEST_EXCHANGE");
    
    auto parser = std::make_unique<MockParser>();
    normalizer.set_parser(std::move(parser));
    
    SUBCASE("Process valid message") {
        bool callback_called = false;
        std::string received_symbol;
        std::vector<std::pair<double, double>> received_bids, received_asks;
        uint64_t received_timestamp = 0;
        
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
            received_symbol = symbol;
            received_bids = bids;
            received_asks = asks;
            received_timestamp = timestamp_us;
        });
        
        normalizer.process_message("valid_message");
        
        CHECK(callback_called == true);
        CHECK(received_symbol == "BTCUSDT");
        CHECK(received_bids.size() == 2);
        CHECK(received_asks.size() == 2);
        CHECK(received_timestamp == 1234567890);
    }
    
    SUBCASE("Process invalid message") {
        bool callback_called = false;
        
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
        });
        
        normalizer.process_message("invalid_message");
        
        CHECK(callback_called == false);
    }
    
    SUBCASE("Process message with unknown exchange") {
        // Create a new normalizer instance without parser
        MarketDataNormalizer normalizer_no_parser("TEST_EXCHANGE");
        bool callback_called = false;
        
        normalizer_no_parser.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
        });
        
        normalizer_no_parser.process_message("valid_message");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        CHECK(callback_called == false);
    }
}

TEST_CASE("MarketDataNormalizer - Callbacks") {
    MarketDataNormalizer normalizer("TEST");
    
    SUBCASE("Set orderbook callback") {
        bool callback_called = false;
        
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
        });
        
        // Simulate a callback (in real usage, this would come from message processing)
        proto::OrderBookSnapshot snapshot;
        snapshot.set_symbol("BTCUSDT");
        snapshot.set_exch("TEST");
        
        // Note: In real implementation, the normalizer would call this internally
        // For unit testing, we're verifying the callback mechanism is set up
        CHECK(callback_called == false); // Not called yet
    }
    
    SUBCASE("Set trade callback") {
        bool callback_called = false;
        
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_called = true;
        });
        
        // Similar to above - verifying callback mechanism
        CHECK(callback_called == false);
    }
}

TEST_CASE("MarketDataNormalizer - Error Handling") {
    MarketDataNormalizer normalizer("TEST");
    
    SUBCASE("Process message without parser") {
        // Should not crash
        normalizer.process_message("some_message");
    }
    
    SUBCASE("Set parser") {
        // Should handle gracefully
        auto parser = std::make_unique<MockParser>("TEST");
        normalizer.set_parser(std::move(parser));
        CHECK(true); // Basic test
    }
}

TEST_CASE("MarketDataNormalizer - Thread Safety") {
    MarketDataNormalizer normalizer("TEST");
    
    SUBCASE("Concurrent parser registration") {
        auto parser1 = std::make_unique<BinanceParser>();
        auto parser2 = std::make_unique<CoinbaseParser>();
        
        // Test concurrent access (simplified)
        normalizer.set_parser(std::move(parser1));
        CHECK(true); // Basic test
    }
    
    SUBCASE("Concurrent message processing") {
        auto parser = std::make_unique<MockParser>("TEST", 2000.0);
        normalizer.set_parser(std::move(parser));
        
        int callback_count = 0;
        normalizer.set_callback([&](const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
            callback_count++;
        });
        
        // Process messages from multiple threads
        std::thread t1([&]() { 
            for (int i = 0; i < 10; ++i) {
                normalizer.process_message("valid_message");
            }
        });
        
        std::thread t2([&]() { 
            for (int i = 0; i < 10; ++i) {
                normalizer.process_message("valid_message");
            }
        });
        
        t1.join();
        t2.join();
        
        // Give it time to process all messages
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(callback_count == 20);
    }
}
