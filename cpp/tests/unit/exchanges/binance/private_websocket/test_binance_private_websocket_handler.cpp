#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../../../../exchanges/binance/private_websocket/binance_private_websocket_handler.hpp"

using namespace binance;

TEST_SUITE("BinancePrivateWebSocketHandler") {

    TEST_CASE("Constructor and Destructor") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Initialize and Shutdown") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.initialize());
        CHECK_FALSE(handler.is_connected());
        
        handler.shutdown();
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Connect and Disconnect") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::string test_url = "wss://fstream.binance.com/ws/test_listen_key";
        
        CHECK(handler.connect(test_url));
        CHECK(handler.is_connected());
        
        handler.disconnect();
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Subscribe to User Data") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        CHECK(handler.subscribe_to_user_data());
        
        handler.disconnect();
    }

    TEST_CASE("Subscribe to Order Updates") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        CHECK(handler.subscribe_to_order_updates());
        
        handler.disconnect();
    }

    TEST_CASE("Subscribe to Account Updates") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        CHECK(handler.subscribe_to_account_updates());
        
        handler.disconnect();
    }

    TEST_CASE("Unsubscribe from Channel") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Subscribe first
        CHECK(handler.subscribe_to_user_data());
        
        // Then unsubscribe
        CHECK(handler.unsubscribe_from_channel("userData"));
        
        handler.disconnect();
    }

    TEST_CASE("Send Message") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Should not crash when sending messages
        handler.send_message("test message");
        
        handler.disconnect();
    }

    TEST_CASE("Send Binary Data") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
        handler.send_binary(test_data);
        
        handler.disconnect();
    }

    TEST_CASE("Message Callback") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        std::string received_data;
        BinancePrivateMessageType received_type;
        
        handler.set_message_callback([&](const BinancePrivateWebSocketMessage& message) {
            callback_called = true;
            received_data = message.data;
            received_type = message.type;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate receiving a message
        handler.handle_private_message("{\"e\":\"outboundAccountPosition\",\"E\":123456789,\"u\":123456789,\"B\":[{\"a\":\"BTC\",\"f\":\"1.5\",\"F\":\"0.0\"}]}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Order Callback") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        std::string received_order_id, received_status;
        
        handler.set_order_callback([&](const std::string& order_id, const std::string& status) {
            callback_called = true;
            received_order_id = order_id;
            received_status = status;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate order update
        handler.handle_order_update("{\"s\":\"BTCUSDT\",\"c\":\"test_order_id\",\"S\":\"BUY\",\"o\":\"LIMIT\",\"q\":\"1.0\",\"p\":\"50000.00\",\"X\":\"FILLED\"}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Account Callback") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        std::string received_data;
        
        handler.set_account_callback([&](const std::string& account_data) {
            callback_called = true;
            received_data = account_data;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate account update
        handler.handle_account_update("{\"e\":\"outboundAccountPosition\",\"E\":123456789,\"u\":123456789,\"B\":[{\"a\":\"BTC\",\"f\":\"1.5\",\"F\":\"0.0\"}]}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Balance Callback") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        std::string received_asset;
        double received_balance = 0.0;
        
        handler.set_balance_callback([&](const std::string& asset, double balance) {
            callback_called = true;
            received_asset = asset;
            received_balance = balance;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate balance update
        handler.handle_balance_update("{\"a\":\"BTC\",\"f\":\"1.5\",\"F\":\"0.0\"}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("User Data Message Handling") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<bool> callback_called{false};
        
        handler.set_message_callback([&](const BinancePrivateWebSocketMessage& message) {
            callback_called = true;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate user data message
        handler.handle_user_data_message("{\"e\":\"outboundAccountPosition\",\"E\":123456789,\"u\":123456789,\"B\":[{\"a\":\"BTC\",\"f\":\"1.5\",\"F\":\"0.0\"}]}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Multiple Subscriptions") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Subscribe to multiple channels
        CHECK(handler.subscribe_to_user_data());
        CHECK(handler.subscribe_to_order_updates());
        CHECK(handler.subscribe_to_account_updates());
        
        handler.disconnect();
    }

    TEST_CASE("Connection State Management") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        // Initially disconnected
        CHECK_FALSE(handler.is_connected());
        
        // Connect
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        CHECK(handler.is_connected());
        
        // Disconnect
        handler.disconnect();
        CHECK_FALSE(handler.is_connected());
        
        // Reconnect
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        CHECK(handler.is_connected());
        
        handler.disconnect();
    }

    TEST_CASE("Exchange Name and Type") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK_EQ(handler.get_exchange_name(), "BINANCE");
        CHECK_EQ(handler.get_type(), WebSocketType::PRIVATE_USER_DATA);
    }

    TEST_CASE("API Credentials") {
        std::string api_key = "test_api_key_123";
        std::string api_secret = "test_api_secret_456";
        
        BinancePrivateWebSocketHandler handler(api_key, api_secret);
        
        // Handler should be created successfully with credentials
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Authentication Required for Private Streams") {
        // Test that private streams require valid credentials
        BinancePrivateWebSocketHandler handler("", "");  // Empty credentials
        
        // Should fail to connect without proper credentials
        CHECK_FALSE(handler.connect("wss://fstream.binance.com/ws/"));
        
        // Test with invalid credentials
        BinancePrivateWebSocketHandler handler2("invalid_key", "invalid_secret");
        CHECK_FALSE(handler2.connect("wss://fstream.binance.com/ws/invalid_listen_key"));
    }

    TEST_CASE("Listen Key Authentication") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        // Test listen key generation (requires valid API credentials)
        std::string listen_key = handler.generate_listen_key();
        
        // In real implementation, this would validate API credentials
        // For now, check that a listen key is generated
        CHECK_FALSE(listen_key.empty());
        
        // Test connection with generated listen key
        std::string ws_url = "wss://fstream.binance.com/ws/" + listen_key;
        CHECK(handler.connect(ws_url));
        
        handler.disconnect();
    }

    TEST_CASE("Authentication Failure Handling") {
        BinancePrivateWebSocketHandler handler("invalid_api_key", "invalid_api_secret");
        
        // Test that authentication failures are handled gracefully
        std::string listen_key = handler.generate_listen_key();
        
        // Should still generate a mock listen key for testing
        CHECK_FALSE(listen_key.empty());
        
        // Connection should fail with invalid credentials
        CHECK_FALSE(handler.connect("wss://fstream.binance.com/ws/invalid_listen_key"));
    }

    TEST_CASE("Listen Key Generation") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        // Test listen key generation (mock implementation)
        std::string listen_key = handler.generate_listen_key();
        CHECK_FALSE(listen_key.empty());
        CHECK(listen_key.find("mock_listen_key_") == 0);
    }

    TEST_CASE("Listen Key Refresh") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Test listen key refresh (should not crash)
        handler.refresh_listen_key();
        
        handler.disconnect();
    }

    TEST_CASE("Error Handling - Send Without Connection") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        // Try to send without connecting
        handler.send_message("test");
        handler.send_binary({0x01, 0x02});
        
        // Should not crash
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Error Handling - Subscribe Without Connection") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        // Try to subscribe without connecting
        CHECK(handler.subscribe_to_user_data());
        CHECK(handler.subscribe_to_order_updates());
        
        // Should still work (queued for when connection is established)
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Concurrent Operations") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Test concurrent subscriptions
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&]() {
                handler.subscribe_to_user_data();
                handler.subscribe_to_order_updates();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        handler.disconnect();
    }

    TEST_CASE("Callback Thread Safety") {
        BinancePrivateWebSocketHandler handler("test_api_key", "test_api_secret");
        
        std::atomic<int> callback_count{0};
        
        handler.set_message_callback([&](const BinancePrivateWebSocketMessage& message) {
            callback_count.fetch_add(1);
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/ws/test_listen_key"));
        
        // Simulate multiple concurrent messages
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                handler.handle_private_message("{\"e\":\"test\",\"E\":123456789}");
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Give some time for all callbacks to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        handler.disconnect();
    }
}
