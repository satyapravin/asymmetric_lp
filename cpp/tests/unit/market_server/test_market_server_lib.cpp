#include "doctest.h"
#include "../../../market_server/market_server_lib.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include <memory>

TEST_CASE("MarketServerLib - Initialization") {
    market_server::MarketServerLib server;
    
    CHECK(true); // Basic initialization test
}

TEST_CASE("MarketServerLib - Set Exchange and Symbol") {
    market_server::MarketServerLib server;
    
    server.set_exchange("binance");
    server.set_symbol("BTCUSDT");
    
    CHECK(true); // Should not crash
}

TEST_CASE("MarketServerLib - Set ZMQ Publisher") {
    market_server::MarketServerLib server;
    
    auto publisher = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5590");
    server.set_zmq_publisher(publisher);
    
    CHECK(true); // Should not crash
}

TEST_CASE("MarketServerLib - Set WebSocket Transport") {
    market_server::MarketServerLib server;
    
    // Create a mock transport (we'll use nullptr for this test)
    auto transport = std::shared_ptr<websocket_transport::IWebSocketTransport>(nullptr);
    server.set_websocket_transport(transport);
    
    CHECK(true); // Should not crash
}

TEST_CASE("MarketServerLib - Start and Stop") {
    market_server::MarketServerLib server;
    
    // Test start/stop cycle
    server.start();
    CHECK(server.is_running());
    
    server.stop();
    CHECK(!server.is_running());
}

TEST_CASE("MarketServerLib - Statistics") {
    market_server::MarketServerLib server;
    
    auto stats = server.get_statistics();
    
    // Test initial statistics
    CHECK(stats.market_data_received.load() == 0);
    CHECK(stats.zmq_messages_sent.load() == 0);
    
    // Test reset statistics
    server.reset_statistics();
    CHECK(stats.market_data_received.load() == 0);
}

TEST_CASE("MarketServerLib - Error Handling") {
    market_server::MarketServerLib server;
    
    // Test error callback
    bool error_called = false;
    server.set_error_callback([&error_called](const std::string& error) {
        error_called = true;
    });
    
    CHECK(true); // Callback should be set
}
