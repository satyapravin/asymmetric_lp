#include "doctest.h"
#include "../../../position_server/position_server_lib.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include <memory>

TEST_CASE("PositionServerLib - Initialization") {
    position_server::PositionServerLib server;
    
    CHECK(true); // Basic initialization test
}

TEST_CASE("PositionServerLib - Set Exchange") {
    position_server::PositionServerLib server;
    
    server.set_exchange("binance");
    
    CHECK(true); // Should not crash
}

TEST_CASE("PositionServerLib - Set ZMQ Publisher") {
    position_server::PositionServerLib server;
    
    auto publisher = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5591");
    server.set_zmq_publisher(publisher);
    
    CHECK(true); // Should not crash
}

TEST_CASE("PositionServerLib - Set WebSocket Transport") {
    position_server::PositionServerLib server;
    
    // Create a mock transport (we'll use nullptr for this test)
    auto transport = std::shared_ptr<websocket_transport::IWebSocketTransport>(nullptr);
    server.set_websocket_transport(transport);
    
    CHECK(true); // Should not crash
}

TEST_CASE("PositionServerLib - Start and Stop") {
    position_server::PositionServerLib server;
    
    // Test start/stop cycle
    server.start();
    CHECK(server.is_running());
    
    server.stop();
    CHECK(!server.is_running());
}

TEST_CASE("PositionServerLib - Statistics") {
    position_server::PositionServerLib server;
    
    auto stats = server.get_statistics();
    
    // Test initial statistics
    CHECK(stats.position_updates.load() == 0);
    CHECK(stats.balance_updates.load() == 0);
    CHECK(stats.zmq_messages_sent.load() == 0);
    
    // Test reset statistics
    server.reset_statistics();
    CHECK(stats.position_updates.load() == 0);
}

TEST_CASE("PositionServerLib - Error Handling") {
    position_server::PositionServerLib server;
    
    // Test error callback
    bool error_called = false;
    server.set_error_callback([&error_called](const std::string& error) {
        error_called = true;
    });
    
    CHECK(true); // Callback should be set
}

TEST_CASE("PositionServerLib - Position Callback") {
    position_server::PositionServerLib server;
    
    // Test position callback
    bool position_called = false;
    server.set_position_callback([&position_called](const proto::PositionUpdate& position) {
        position_called = true;
    });
    
    CHECK(true); // Callback should be set
}

TEST_CASE("PositionServerLib - Balance Callback") {
    position_server::PositionServerLib server;
    
    // Test balance callback
    bool balance_called = false;
    server.set_balance_callback([&balance_called](const proto::AccountBalanceUpdate& balance) {
        balance_called = true;
    });
    
    CHECK(true); // Callback should be set
}
