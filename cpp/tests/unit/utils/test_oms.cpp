#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../utils/oms/enhanced_oms.hpp"
#include "../utils/oms/mock_exchange_oms.hpp"
#include "../utils/oms/order.hpp"
#include "../utils/oms/types.hpp"
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
    
    TEST_CASE("Connect All Exchanges") {
        OMS oms;
        auto mock_oms1 = std::make_shared<MockExchangeOMS>("EXCHANGE_1", 0.8, 0.1, std::chrono::milliseconds(100));
        auto mock_oms2 = std::make_shared<MockExchangeOMS>("EXCHANGE_2", 0.8, 0.1, std::chrono::milliseconds(100));
        
        oms.register_exchange("EXCHANGE_1", mock_oms1);
        oms.register_exchange("EXCHANGE_2", mock_oms2);
        
        oms.connect_all_exchanges();
        
        // Both exchanges should be connected
        CHECK(mock_oms1->is_connected());
        CHECK(mock_oms2->is_connected());
    }
    
    TEST_CASE("Disconnect All Exchanges") {
        OMS oms;
        auto mock_oms1 = std::make_shared<MockExchangeOMS>("EXCHANGE_1", 0.8, 0.1, std::chrono::milliseconds(100));
        auto mock_oms2 = std::make_shared<MockExchangeOMS>("EXCHANGE_2", 0.8, 0.1, std::chrono::milliseconds(100));
        
        oms.register_exchange("EXCHANGE_1", mock_oms1);
        oms.register_exchange("EXCHANGE_2", mock_oms2);
        
        oms.connect_all_exchanges();
        oms.disconnect_all_exchanges();
        
        // Both exchanges should be disconnected
        CHECK(!mock_oms1->is_connected());
        CHECK(!mock_oms2->is_connected());
    }
    
    TEST_CASE("Send Order to Specific Exchange") {
        OMS oms;
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 1.0, 0.0, std::chrono::milliseconds(10));
        
        oms.register_exchange("TEST_EXCHANGE", mock_oms);
        oms.connect_all_exchanges();
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_001";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Buy;
        order.qty = 0.1;
        order.price = 50000.0;
        
        std::vector<OrderEvent> events;
        mock_oms->on_order_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        oms.send_order(order);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have received events
        CHECK(events.size() > 0);
        CHECK(events[0].cl_ord_id == "TEST_ORDER_001");
        
        oms.disconnect_all_exchanges();
    }
    
    TEST_CASE("Cancel Order") {
        OMS oms;
        auto mock_oms = std::make_shared<MockExchangeOMS>("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10));
        
        oms.register_exchange("TEST_EXCHANGE", mock_oms);
        oms.connect_all_exchanges();
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_002";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Sell;
        order.qty = 0.2;
        order.price = 50001.0;
        
        std::vector<OrderEvent> events;
        mock_oms->on_order_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        oms.send_order(order);
        
        // Wait for acknowledgment
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        oms.cancel_order("TEST_EXCHANGE", "TEST_ORDER_002", "EXCHANGE_ORDER_ID");
        
        // Wait for cancellation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have acknowledgment and cancellation events
        CHECK(events.size() >= 2);
        
        bool has_cancel = false;
        for (const auto& event : events) {
            if (event.type == OrderEventType::Cancel) has_cancel = true;
        }
        
        CHECK(has_cancel);
        
        oms.disconnect_all_exchanges();
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
        oms.send_order(order);
        
        // Should not crash when cancelling from non-existent exchange
        oms.cancel_order("NON_EXISTENT", "TEST_ORDER_004", "EXCHANGE_ORDER_ID");
        
        CHECK(true); // If we get here, no crash occurred
    }
}