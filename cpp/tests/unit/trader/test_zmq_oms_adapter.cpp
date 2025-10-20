#include "doctest.h"
#include "../../../trader/zmq_oms_adapter.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include "../../../utils/zmq/zmq_subscriber.hpp"
#include <thread>
#include <chrono>

TEST_CASE("ZmqOMSAdapter - Initialization") {
    ZmqOMSAdapter adapter("tcp://127.0.0.1:5580", "orders", "tcp://127.0.0.1:5581", "order_events");
    
    // Test basic initialization
    CHECK(true);
}

TEST_CASE("ZmqOMSAdapter - Send Order") {
    ZmqOMSAdapter adapter("tcp://127.0.0.1:5582", "orders", "tcp://127.0.0.1:5583", "order_events");
    
    // Test sending order
    bool result = adapter.send_order("TEST_ORDER_1", "binance", "BTCUSDT", 0, 0, 0.1, 50000.0);
    CHECK(result == true);
}

TEST_CASE("ZmqOMSAdapter - Cancel Order") {
    ZmqOMSAdapter adapter("tcp://127.0.0.1:5584", "orders", "tcp://127.0.0.1:5585", "order_events");
    
    // Test canceling order
    bool result = adapter.cancel_order("TEST_ORDER_1", "binance");
    CHECK(result == true);
}

TEST_CASE("ZmqOMSAdapter - Event Callback") {
    ZmqOMSAdapter adapter("tcp://127.0.0.1:5586", "orders", "tcp://127.0.0.1:5587", "order_events");
    
    // Test setting event callback
    bool callback_called = false;
    adapter.set_event_callback([&callback_called](const std::string& cl_ord_id, const std::string& exch, 
                                                const std::string& symbol, uint32_t event_type, 
                                                double fill_qty, double fill_price, const std::string& text) {
        callback_called = true;
    });
    
    CHECK(true); // Callback should be set without error
}

TEST_CASE("ZmqOMSAdapter - Poll Events") {
    ZmqOMSAdapter adapter("tcp://127.0.0.1:5588", "orders", "tcp://127.0.0.1:5589", "order_events");
    
    // Test polling events (should not crash)
    adapter.poll_events();
    CHECK(true);
}
