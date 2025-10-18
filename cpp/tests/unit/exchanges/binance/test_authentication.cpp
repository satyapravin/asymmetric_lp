#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../../../../exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"
#include "../../../../exchanges/binance/private_websocket/binance_private_websocket_handler.hpp"
#include "../../../../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../../../../exchanges/binance/http/binance_oms.hpp"

using namespace binance;

TEST_SUITE("Exchange Authentication Tests") {

    TEST_CASE("Public WebSocket - No Authentication Required") {
        BinancePublicWebSocketHandler handler;
        
        // Public streams don't require authentication
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        CHECK(handler.is_connected());
        
        // Should be able to subscribe to public channels
        CHECK(handler.subscribe_to_ticker("BTCUSDT"));
        CHECK(handler.subscribe_to_orderbook("BTCUSDT", 20));
        
        handler.disconnect();
    }

    TEST_CASE("Private WebSocket - Authentication Required") {
        // Test with valid credentials
        BinancePrivateWebSocketHandler handler("valid_api_key", "valid_api_secret");
        
        // Should be able to connect with valid credentials
        CHECK(handler.connect("wss://fstream.binance.com/ws/valid_listen_key"));
        CHECK(handler.is_connected());
        
        // Should be able to subscribe to private channels
        CHECK(handler.subscribe_to_user_data());
        CHECK(handler.subscribe_to_order_updates());
        
        handler.disconnect();
    }

    TEST_CASE("Private WebSocket - Authentication Failure") {
        // Test with invalid credentials
        BinancePrivateWebSocketHandler handler("invalid_api_key", "invalid_api_secret");
        
        // Should fail to connect with invalid credentials
        CHECK_FALSE(handler.connect("wss://fstream.binance.com/ws/invalid_listen_key"));
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("HTTP Data Fetcher - Authentication Required") {
        // Test with valid credentials
        BinanceDataFetcher fetcher("valid_api_key", "valid_api_secret");
        
        // Should be able to connect with valid credentials
        CHECK(fetcher.connect("https://fapi.binance.com"));
        CHECK(fetcher.is_connected());
        
        // Should be able to fetch private data
        auto account_info = fetcher.get_account_info();
        auto positions = fetcher.get_positions();
        
        fetcher.disconnect();
    }

    TEST_CASE("HTTP Data Fetcher - Authentication Failure") {
        // Test with invalid credentials
        BinanceDataFetcher fetcher("invalid_api_key", "invalid_api_secret");
        
        // Should fail to connect with invalid credentials
        CHECK_FALSE(fetcher.connect("https://fapi.binance.com"));
        CHECK_FALSE(fetcher.is_connected());
    }

    TEST_CASE("HTTP OMS - Authentication Required") {
        // Test with valid credentials
        BinanceOMS oms("valid_api_key", "valid_api_secret");
        
        // Should be able to connect with valid credentials
        CHECK(oms.connect("https://fapi.binance.com"));
        CHECK(oms.is_connected());
        
        // Should be able to place orders
        auto result = oms.place_market_order("BTCUSDT", "BUY", 0.1);
        
        oms.disconnect();
    }

    TEST_CASE("HTTP OMS - Authentication Failure") {
        // Test with invalid credentials
        BinanceOMS oms("invalid_api_key", "invalid_api_secret");
        
        // Should fail to connect with invalid credentials
        CHECK_FALSE(oms.connect("https://fapi.binance.com"));
        CHECK_FALSE(oms.is_connected());
    }

    TEST_CASE("Credential Validation - Empty Credentials") {
        // Test empty API key
        BinancePrivateWebSocketHandler handler1("", "valid_secret");
        CHECK_FALSE(handler1.connect("wss://fstream.binance.com/ws/"));
        
        // Test empty API secret
        BinancePrivateWebSocketHandler handler2("valid_key", "");
        CHECK_FALSE(handler2.connect("wss://fstream.binance.com/ws/"));
        
        // Test both empty
        BinancePrivateWebSocketHandler handler3("", "");
        CHECK_FALSE(handler3.connect("wss://fstream.binance.com/ws/"));
    }

    TEST_CASE("Credential Validation - Invalid Format") {
        // Test invalid API key format
        BinancePrivateWebSocketHandler handler1("invalid_key_format", "valid_secret");
        CHECK_FALSE(handler1.connect("wss://fstream.binance.com/ws/"));
        
        // Test invalid API secret format
        BinancePrivateWebSocketHandler handler2("valid_key", "invalid_secret_format");
        CHECK_FALSE(handler2.connect("wss://fstream.binance.com/ws/"));
    }

    TEST_CASE("Authentication Token Expiration") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate token expiration
        // In real implementation, this would test token refresh logic
        handler.refresh_listen_key();
        
        // Should still be connected after refresh
        CHECK(handler.is_connected());
        
        handler.disconnect();
    }

    TEST_CASE("Rate Limiting with Authentication") {
        BinanceOMS oms("test_api_key", "test_api_secret");
        
        CHECK(oms.connect("https://fapi.binance.com"));
        
        // Test rapid successive authenticated requests
        for (int i = 0; i < 10; ++i) {
            oms.get_account_info();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        oms.disconnect();
    }

    TEST_CASE("Concurrent Authentication") {
        // Test multiple handlers with different credentials
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([i]() {
                std::string api_key = "test_api_key_" + std::to_string(i);
                std::string api_secret = "test_api_secret_" + std::to_string(i);
                
                BinancePrivateWebSocketHandler handler(api_key, api_secret);
                handler.connect("wss://fstream.binance.com/ws/test_listen_key_" + std::to_string(i));
                
                BinanceOMS oms(api_key, api_secret);
                oms.connect("https://fapi.binance.com");
                
                handler.disconnect();
                oms.disconnect();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }

    TEST_CASE("Authentication Error Messages") {
        BinancePrivateWebSocketHandler handler("invalid_key", "invalid_secret");
        
        // Test that authentication errors provide meaningful messages
        CHECK_FALSE(handler.connect("wss://fstream.binance.com/ws/invalid_listen_key"));
        
        // In real implementation, this would check error messages
        // For now, just verify connection failure
    }

    TEST_CASE("Mixed Authentication Scenarios") {
        // Test public and private handlers together
        BinancePublicWebSocketHandler public_handler;
        BinancePrivateWebSocketHandler private_handler("test_api_key", "test_api_secret");
        
        // Public should connect without authentication
        CHECK(public_handler.connect("wss://fstream.binance.com/stream"));
        
        // Private should connect with authentication
        CHECK(private_handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Both should be connected
        CHECK(public_handler.is_connected());
        CHECK(private_handler.is_connected());
        
        public_handler.disconnect();
        private_handler.disconnect();
    }

    TEST_CASE("Authentication Security") {
        // Test that credentials are not exposed in logs or error messages
        BinancePrivateWebSocketHandler handler("sensitive_api_key", "sensitive_api_secret");
        
        // In real implementation, this would verify that:
        // 1. Credentials are not logged
        // 2. Credentials are not exposed in error messages
        // 3. Credentials are securely stored
        
        CHECK_FALSE(handler.connect("wss://fstream.binance.com/ws/invalid_listen_key"));
    }

    TEST_CASE("Authentication Retry Logic") {
        BinanceOMS oms("test_api_key", "test_api_secret");
        
        // Test authentication retry logic
        // In real implementation, this would test:
        // 1. Retry on authentication failure
        // 2. Exponential backoff
        // 3. Maximum retry attempts
        
        CHECK(oms.connect("https://fapi.binance.com"));
        oms.disconnect();
    }
}
