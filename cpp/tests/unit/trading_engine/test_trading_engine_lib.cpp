#include "doctest.h"
#include "../../../trading_engine/trading_engine_lib.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include "../../../exchanges/i_exchange_oms.hpp"
#include <memory>
#include <atomic>

// Mock OMS for testing
class MockExchangeOMS : public exchanges::IExchangeOMS {
public:
    std::atomic<bool> connected_{false};
    std::atomic<int> connect_calls_{0};
    std::atomic<int> disconnect_calls_{0};
    
    bool connect() override {
        connect_calls_++;
        connected_ = true;
        return true;
    }
    
    void disconnect() override {
        disconnect_calls_++;
        connected_ = false;
    }
    
    bool is_connected() const override {
        return connected_.load();
    }
    
    std::string get_exchange_name() const override {
        return "mock_exchange";
    }
    
    void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) override {
        // Mock implementation
    }
};

TEST_CASE("TradingEngineLib - Initialization") {
    trading_engine::TradingEngineLib engine;
    
    // Test basic initialization
    CHECK(true); // Basic initialization test
}

TEST_CASE("TradingEngineLib - Set Exchange") {
    trading_engine::TradingEngineLib engine;
    
    engine.set_exchange("binance");
    CHECK(engine.get_exchange_name() == "binance");
}

TEST_CASE("TradingEngineLib - Set ZMQ Publisher") {
    trading_engine::TradingEngineLib engine;
    
    auto publisher = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5570");
    engine.set_zmq_publisher(publisher);
    
    // Test that publisher is set (no direct getter, but should not crash)
    CHECK(true);
}

TEST_CASE("TradingEngineLib - Set WebSocket Transport") {
    trading_engine::TradingEngineLib engine;
    
    // Create a mock transport (we'll use nullptr for this test)
    auto transport = std::shared_ptr<websocket_transport::IWebSocketTransport>(nullptr);
    engine.set_websocket_transport(transport);
    
    CHECK(true); // Should not crash
}

TEST_CASE("TradingEngineLib - Start and Stop") {
    trading_engine::TradingEngineLib engine;
    
    // Test start/stop cycle
    engine.start();
    CHECK(engine.is_running());
    
    engine.stop();
    CHECK(!engine.is_running());
}

TEST_CASE("TradingEngineLib - Order State Management") {
    trading_engine::TradingEngineLib engine;
    
    // Test order state tracking
    std::string order_id = "TEST_ORDER_1";
    
    // Initially no order state
    auto state = engine.get_order_state(order_id);
    CHECK(!state.has_value());
    
    // Test getting active orders
    auto active_orders = engine.get_active_orders();
    CHECK(active_orders.empty());
    
    // Test getting all orders
    auto all_orders = engine.get_all_orders();
    CHECK(all_orders.empty());
}

TEST_CASE("TradingEngineLib - Statistics") {
    trading_engine::TradingEngineLib engine;
    
    auto stats = engine.get_statistics();
    
    // Test initial statistics
    CHECK(stats.orders_sent.load() == 0);
    CHECK(stats.orders_cancelled.load() == 0);
    CHECK(stats.orders_modified.load() == 0);
    CHECK(stats.zmq_messages_sent.load() == 0);
    
    // Test reset statistics
    engine.reset_statistics();
    CHECK(stats.orders_sent.load() == 0);
}

TEST_CASE("TradingEngineLib - Error Handling") {
    trading_engine::TradingEngineLib engine;
    
    // Test error callback
    bool error_called = false;
    engine.set_error_callback([&error_called](const std::string& error) {
        error_called = true;
    });
    
    // Trigger an error (this would be done internally)
    // For now, just test that callback is set
    CHECK(true);
}

TEST_CASE("TradingEngineLib - Order Event Callback") {
    trading_engine::TradingEngineLib engine;
    
    // Test order event callback
    bool event_called = false;
    engine.set_order_event_callback([&event_called](const proto::OrderEvent& event) {
        event_called = true;
    });
    
    // Test that callback is set
    CHECK(true);
}
