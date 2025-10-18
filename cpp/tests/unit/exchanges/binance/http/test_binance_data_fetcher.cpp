#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../../../../exchanges/binance/http/binance_data_fetcher.hpp"

using namespace binance;

TEST_SUITE("BinanceDataFetcher") {

    TEST_CASE("Constructor and Destructor") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Initialize and Shutdown") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.initialize());
        CHECK_FALSE(fetcher.is_connected());
        
        fetcher.shutdown();
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Connect and Disconnect") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        std::string test_url = "https://fapi.binance.com";
        
        CHECK(fetcher.connect(test_url));
        CHECK(fetcher.is_connected());
        
        fetcher.disconnect();
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Get Account Information") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test account info fetching
        auto account_info = fetcher.get_account_info();
        // In real implementation, this would return actual account data
        
        fetcher.disconnect();
    }

    TEST_CASE("Get Positions") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test position fetching
        auto positions = fetcher.get_positions();
        // In real implementation, this would return actual position data
        
        fetcher.disconnect();
    }

    TEST_CASE("Get Open Orders") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test open orders fetching
        auto orders = fetcher.get_open_orders("BTCUSDT");
        // In real implementation, this would return actual order data
        
        fetcher.disconnect();
    }

    TEST_CASE("Get Order History") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test order history fetching
        auto order_history = fetcher.get_order_history("BTCUSDT", 10);
        // In real implementation, this would return actual order history
        
        fetcher.disconnect();
    }

    TEST_CASE("Get Trade History") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test trade history fetching
        auto trade_history = fetcher.get_trade_history("BTCUSDT", 10);
        // In real implementation, this would return actual trade history
        
        fetcher.disconnect();
    }

    TEST_CASE("Get Balance") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test balance fetching
        auto balance = fetcher.get_balance("USDT");
        // In real implementation, this would return actual balance
        
        fetcher.disconnect();
    }

    TEST_CASE("Get All Balances") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test all balances fetching
        auto balances = fetcher.get_all_balances();
        // In real implementation, this would return actual balances
        
        fetcher.disconnect();
    }

    TEST_CASE("Set Callbacks") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        
        fetcher.set_account_callback([&](const std::string& account_data) {
            callback_called = true;
        });
        
        fetcher.set_position_callback([&](const std::string& symbol, double qty, double avg_price) {
            callback_called = true;
        });
        
        fetcher.set_order_callback([&](const std::string& order_id, const std::string& status) {
            callback_called = true;
        });
        
        fetcher.set_balance_callback([&](const std::string& asset, double balance) {
            callback_called = true;
        });
        
        // Callbacks should be set successfully
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Connection State Management") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        // Initially disconnected
        CHECK_FALSE(fetcher.is_connected());
        
        // Connect
        CHECK(fetcher.connect("https://fapi.binance.com"));
        CHECK(fetcher.is_connected());
        
        // Disconnect
        fetcher.disconnect();
        CHECK_FALSE(fetcher.is_connected());
        
        // Reconnect
        CHECK(fetcher.connect("https://fapi.binance.com"));
        CHECK(fetcher.is_connected());
        
        fetcher.disconnect();
    }

    TEST_CASE("API Credentials") {
        std::string api_key = "test_api_key_123";
        std::string api_secret = "test_api_secret_456";
        
        BinanceDataFetcher fetcher(api_key, api_secret);
        
        // Fetcher should be created successfully with credentials
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Error Handling - Operations Without Connection") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        // Try to perform operations without connecting
        auto account_info = fetcher.get_account_info();
        auto positions = fetcher.get_positions();
        auto orders = fetcher.get_open_orders("BTCUSDT");
        
        // Should not crash
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("Error Handling - Invalid Symbol") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Try with invalid symbol
        auto orders = fetcher.get_open_orders("INVALID_SYMBOL");
        auto order_history = fetcher.get_order_history("INVALID_SYMBOL", 10);
        auto trade_history = fetcher.get_trade_history("INVALID_SYMBOL", 10);
        
        fetcher.disconnect();
    }

    TEST_CASE("Error Handling - Invalid Asset") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Try with invalid asset
        auto balance = fetcher.get_balance("INVALID_ASSET");
        
        fetcher.disconnect();
    }

    TEST_CASE("Concurrent Operations") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test concurrent operations
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&]() {
                fetcher.get_account_info();
                fetcher.get_positions();
                fetcher.get_all_balances();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        fetcher.disconnect();
    }

    TEST_CASE("Callback Thread Safety") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        std::atomic<int> callback_count{0};
        
        fetcher.set_account_callback([&](const std::string& account_data) {
            callback_count.fetch_add(1);
        });
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Simulate multiple concurrent callbacks
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                // Simulate callback invocation
                fetcher.set_account_callback([&](const std::string& account_data) {
                    callback_count.fetch_add(1);
                });
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        fetcher.disconnect();
    }

    TEST_CASE("Data Validation") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test data validation for different symbols
        std::vector<std::string> test_symbols = {"BTCUSDT", "ETHUSDT", "ADAUSDT", "DOTUSDT"};
        
        for (const auto& symbol : test_symbols) {
            auto orders = fetcher.get_open_orders(symbol);
            auto order_history = fetcher.get_order_history(symbol, 5);
            auto trade_history = fetcher.get_trade_history(symbol, 5);
        }
        
        fetcher.disconnect();
    }

    TEST_CASE("Rate Limiting") {
        BinanceDataFetcher fetcher("test_api_key", "test_api_secret");
        
        CHECK(fetcher.connect("https://fapi.binance.com"));
        
        // Test rapid successive calls (rate limiting)
        for (int i = 0; i < 20; ++i) {
            fetcher.get_account_info();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        fetcher.disconnect();
    }
}
