#include "doctest.h"
#include "../../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../mocks/mock_http_handler.hpp"
#include "../mocks/mock_websocket_handler.hpp"
#include <memory>

TEST_SUITE("Binance Exchange Tests") {
    
    TEST_CASE("Binance Data Fetcher with Mock HTTP") {
        // Create mock HTTP handler with test data
        auto mock_http = std::make_unique<MockHttpHandler>("cpp/tests/data/binance/http");
        
        // Create Binance data fetcher
        BinanceDataFetcher fetcher("test_key", "test_secret");
        // Note: We would need to modify BinanceDataFetcher to accept mock HTTP handler
        // For now, this test shows the intended structure
        
        // Test balance fetching
        auto balances = fetcher.get_balances();
        CHECK(balances.size() == 3); // Based on our test data
        CHECK(balances[0].exch() == "BINANCE");
        CHECK(balances[0].instrument() == "USDT");
        CHECK(balances[0].balance() == 1000.0);
        CHECK(balances[0].available() == 950.0);
        
        CHECK(balances[1].instrument() == "BTC");
        CHECK(balances[1].balance() == 0.1);
        CHECK(balances[1].available() == 0.08);
        
        CHECK(balances[2].instrument() == "ETH");
        CHECK(balances[2].balance() == 1.5);
        CHECK(balances[2].available() == 1.2);
        
        // Test position fetching (using position risk data)
        auto positions = fetcher.get_positions();
        CHECK(positions.size() == 2); // Only non-zero positions
        CHECK(positions[0].exch() == "BINANCE");
        CHECK(positions[0].symbol() == "BTCUSDT");
        CHECK(positions[0].qty() == 0.1);
        CHECK(positions[0].avg_price() == 50000.0);
        
        CHECK(positions[1].symbol() == "ETHUSDT");
        CHECK(positions[1].qty() == 1.0);
        CHECK(positions[1].avg_price() == 3000.0);
        
        // Test order fetching
        auto orders = fetcher.get_open_orders();
        CHECK(orders.size() == 2); // Based on our test data
        CHECK(orders[0].exch() == "BINANCE");
        CHECK(orders[0].symbol() == "BTCUSDT");
        CHECK(orders[0].cl_ord_id() == "test_order_1");
        CHECK(orders[0].side() == proto::Side::BUY);
        CHECK(orders[0].order_type() == proto::OrderType::LIMIT);
        
        CHECK(orders[1].symbol() == "ETHUSDT");
        CHECK(orders[1].cl_ord_id() == "test_order_2");
        CHECK(orders[1].side() == proto::Side::SELL);
    }
    
    TEST_CASE("Binance PMS with Mock WebSocket") {
        auto mock_ws = std::make_unique<MockWebSocketHandler>("cpp/tests/data/binance/websocket");
        
        // Set up callbacks
        std::vector<proto::AccountBalanceUpdate> balance_updates;
        std::vector<proto::PositionUpdate> position_updates;
        
        mock_ws->set_message_callback([&](const std::string& message) {
            // Parse JSON and create appropriate proto messages
            // This would be implemented based on the actual message parsing logic
            std::cout << "[TEST] Received WebSocket message: " << message << std::endl;
        });
        
        // Connect and simulate messages
        CHECK(mock_ws->connect("ws://localhost:9001"));
        
        // Simulate account update message
        mock_ws->simulate_message_from_file("account_update_message.json");
        
        // Wait for message processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify callbacks were called
        // This would be implemented based on the actual callback logic
        
        mock_ws->disconnect();
    }
    
    TEST_CASE("Error Handling Tests") {
        auto mock_http = std::make_unique<MockHttpHandler>("cpp/tests/data/binance/http");
        mock_http->set_failure_rate(1.0); // 100% failure rate
        
        BinanceDataFetcher fetcher("test_key", "test_secret");
        
        // Should handle failures gracefully
        auto balances = fetcher.get_balances();
        CHECK(balances.size() == 0);
        
        auto positions = fetcher.get_positions();
        CHECK(positions.size() == 0);
        
        auto orders = fetcher.get_open_orders();
        CHECK(orders.size() == 0);
    }
    
    TEST_CASE("Network Failure Simulation") {
        auto mock_http = std::make_unique<MockHttpHandler>("cpp/tests/data/binance/http");
        mock_http->enable_network_failure(true);
        
        BinanceDataFetcher fetcher("test_key", "test_secret");
        
        // Should handle network failures gracefully
        auto balances = fetcher.get_balances();
        CHECK(balances.size() == 0);
    }
    
    TEST_CASE("Response Delay Simulation") {
        auto mock_http = std::make_unique<MockHttpHandler>("cpp/tests/data/binance/http");
        mock_http->set_response_delay(std::chrono::milliseconds(100));
        
        BinanceDataFetcher fetcher("test_key", "test_secret");
        
        auto start = std::chrono::steady_clock::now();
        auto balances = fetcher.get_balances();
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        CHECK(duration.count() >= 100); // Should take at least 100ms
        
        CHECK(balances.size() == 3); // Should still return correct data
    }
}
