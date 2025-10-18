#include "doctest.h"
#include "../../utils/oms/oms.hpp"
#include "../../utils/oms/mock_exchange_oms.hpp"
#include "../../utils/oms/order.hpp"
#include "../../utils/oms/types.hpp"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

TEST_SUITE("OMS") {
    
    TEST_CASE("Constructor and Basic Properties") {
        OMS oms;
        // OMS should be constructible
        CHECK(true);
    }
    
    TEST_CASE("Register Exchange") {
        OMS oms;
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        
        oms.register_exchange("TEST_EXCHANGE", mock_oms);
        
        // Exchange should be registered
        CHECK(true); // No direct way to check registration, but no exception thrown
    }
    
    TEST_CASE("Send Order to Specific Exchange") {
        OMS oms;
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 1.0, 0.0, std::chrono::milliseconds(10));
        
        oms.register_exchange("TEST_EXCHANGE", mock_oms);
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_001";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Buy;
        order.qty = 0.1;
        order.price = 50000.0;
        
        std::vector<OrderEvent> events;
        mock_oms->on_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        oms.send(order);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have received events
        CHECK(events.size() > 0);
        CHECK(events[0].cl_ord_id == "TEST_ORDER_001");
    }
    
    TEST_CASE("Cancel Order") {
        OMS oms;
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        
        oms.register_exchange("TEST_EXCHANGE", mock_oms);
        
        // Connect the exchange to enable order processing
        mock_oms->connect();
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_002";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Sell;
        order.qty = 0.2;
        order.price = 50001.0;
        
        std::vector<OrderEvent> events;
        mock_oms->on_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        oms.send(order);
        
        // Wait for acknowledgment
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        oms.cancel("TEST_EXCHANGE", "TEST_ORDER_002");
        
        // Wait for cancellation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have acknowledgment and cancellation events
        CHECK(events.size() >= 2);
        
        bool has_cancel = false;
        for (const auto& event : events) {
            if (event.type == OrderEventType::Cancel) has_cancel = true;
        }
        
        CHECK(has_cancel);
    }
    
    TEST_CASE("Handle Non-existent Exchange") {
        OMS oms;
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_004";
        order.exch = "NON_EXISTENT";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Buy;
        order.qty = 0.1;
        order.price = 50000.0;
        
        // Should not crash when sending to non-existent exchange
        oms.send(order);
        
        // Should not crash when cancelling from non-existent exchange
        oms.cancel("NON_EXISTENT", "TEST_ORDER_004");
        
        CHECK(true); // If we get here, no crash occurred
    }
}