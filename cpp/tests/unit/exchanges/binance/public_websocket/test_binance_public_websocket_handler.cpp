#include <doctest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../../../../exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"

using namespace binance;

TEST_SUITE("BinancePublicWebSocketHandler") {

    TEST_CASE("Constructor and Destructor") {
        BinancePublicWebSocketHandler handler;
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Initialize and Shutdown") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.initialize());
        CHECK_FALSE(handler.is_connected());
        
        handler.shutdown();
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Connect and Disconnect") {
        BinancePublicWebSocketHandler handler;
        
        std::string test_url = "wss://fstream.binance.com/stream";
        
        CHECK(handler.connect(test_url));
        CHECK(handler.is_connected());
        
        handler.disconnect();
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Subscribe to Orderbook") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        CHECK(handler.subscribe_to_orderbook("BTCUSDT", 20));
        CHECK(handler.subscribe_to_orderbook("ETHUSDT", 10));
        
        handler.disconnect();
    }

    TEST_CASE("Subscribe to Ticker") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        CHECK(handler.subscribe_to_ticker("BTCUSDT"));
        CHECK(handler.subscribe_to_ticker("ETHUSDT"));
        
        handler.disconnect();
    }

    TEST_CASE("Subscribe to Trades") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        CHECK(handler.subscribe_to_trades("BTCUSDT"));
        CHECK(handler.subscribe_to_trades("ETHUSDT"));
        
        handler.disconnect();
    }

    TEST_CASE("Subscribe to Kline") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        CHECK(handler.subscribe_to_kline("BTCUSDT", "1m"));
        CHECK(handler.subscribe_to_kline("ETHUSDT", "5m"));
        CHECK(handler.subscribe_to_kline("ADAUSDT", "1h"));
        
        handler.disconnect();
    }

    TEST_CASE("Unsubscribe from Channel") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Subscribe first
        CHECK(handler.subscribe_to_ticker("BTCUSDT"));
        
        // Then unsubscribe
        CHECK(handler.unsubscribe_from_channel("BTCUSDT@ticker"));
        
        handler.disconnect();
    }

    TEST_CASE("Send Message") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Should not crash when sending messages
        handler.send_message("test message");
        
        handler.disconnect();
    }

    TEST_CASE("Send Binary Data") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
        handler.send_binary(test_data);
        
        handler.disconnect();
    }

    TEST_CASE("Message Callback") {
        BinancePublicWebSocketHandler handler;
        
        std::atomic<bool> callback_called{false};
        std::string received_data;
        
        handler.set_message_callback([&](const BinancePublicWebSocketMessage& message) {
            callback_called = true;
            received_data = message.data;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Simulate receiving a message
        handler.handle_public_message("{\"stream\":\"btcusdt@ticker\",\"data\":{\"E\":123456789,\"s\":\"BTCUSDT\",\"c\":\"50000.00\"}}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Orderbook Callback") {
        BinancePublicWebSocketHandler handler;
        
        std::atomic<bool> callback_called{false};
        std::string received_symbol;
        std::vector<std::pair<double, double>> received_bids, received_asks;
        
        handler.set_orderbook_callback([&](const std::string& symbol, 
                                          const std::vector<std::pair<double, double>>& bids,
                                          const std::vector<std::pair<double, double>>& asks) {
            callback_called = true;
            received_symbol = symbol;
            received_bids = bids;
            received_asks = asks;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Simulate orderbook update
        handler.handle_orderbook_update("BTCUSDT", "{\"bids\":[[\"50000.00\",\"1.5\"]],\"asks\":[[\"50001.00\",\"2.0\"]]}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Ticker Callback") {
        BinancePublicWebSocketHandler handler;
        
        std::atomic<bool> callback_called{false};
        std::string received_symbol;
        double received_price = 0.0, received_volume = 0.0;
        
        handler.set_ticker_callback([&](const std::string& symbol, double price, double volume) {
            callback_called = true;
            received_symbol = symbol;
            received_price = price;
            received_volume = volume;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Simulate ticker update
        handler.handle_ticker_update("BTCUSDT", "{\"c\":\"50000.00\",\"v\":\"100.5\"}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Trade Callback") {
        BinancePublicWebSocketHandler handler;
        
        std::atomic<bool> callback_called{false};
        std::string received_symbol;
        double received_price = 0.0, received_qty = 0.0;
        
        handler.set_trade_callback([&](const std::string& symbol, double price, double qty) {
            callback_called = true;
            received_symbol = symbol;
            received_price = price;
            received_qty = qty;
        });
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Simulate trade update
        handler.handle_trade_update("BTCUSDT", "{\"p\":\"50000.00\",\"q\":\"0.1\"}");
        
        // Give some time for callback to be called
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        handler.disconnect();
    }

    TEST_CASE("Multiple Subscriptions") {
        BinancePublicWebSocketHandler handler;
        
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        
        // Subscribe to multiple channels
        CHECK(handler.subscribe_to_orderbook("BTCUSDT", 20));
        CHECK(handler.subscribe_to_ticker("BTCUSDT"));
        CHECK(handler.subscribe_to_trades("BTCUSDT"));
        CHECK(handler.subscribe_to_kline("BTCUSDT", "1m"));
        
        // Subscribe to different symbol
        CHECK(handler.subscribe_to_orderbook("ETHUSDT", 10));
        CHECK(handler.subscribe_to_ticker("ETHUSDT"));
        
        handler.disconnect();
    }

    TEST_CASE("Connection State Management") {
        BinancePublicWebSocketHandler handler;
        
        // Initially disconnected
        CHECK_FALSE(handler.is_connected());
        
        // Connect
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        CHECK(handler.is_connected());
        
        // Disconnect
        handler.disconnect();
        CHECK_FALSE(handler.is_connected());
        
        // Reconnect
        CHECK(handler.connect("wss://fstream.binance.com/stream"));
        CHECK(handler.is_connected());
        
        handler.disconnect();
    }

    TEST_CASE("Exchange Name and Type") {
        BinancePublicWebSocketHandler handler;
        
        CHECK_EQ(handler.get_exchange_name(), "BINANCE");
        CHECK_EQ(handler.get_type(), WebSocketType::PUBLIC_MARKET_DATA);
    }

    TEST_CASE("Error Handling - Send Without Connection") {
        BinancePublicWebSocketHandler handler;
        
        // Try to send without connecting
        handler.send_message("test");
        handler.send_binary({0x01, 0x02});
        
        // Should not crash
        CHECK_FALSE(handler.is_connected());
    }

    TEST_CASE("Error Handling - Subscribe Without Connection") {
        BinancePublicWebSocketHandler handler;
        
        // Try to subscribe without connecting
        CHECK(handler.subscribe_to_ticker("BTCUSDT"));
        CHECK(handler.subscribe_to_orderbook("BTCUSDT", 20));
        
        // Should still work (queued for when connection is established)
        CHECK_FALSE(handler.is_connected());
    }
}
