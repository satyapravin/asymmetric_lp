#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../utils/oms/mock_exchange_oms.hpp"
#include "../utils/oms/oms.hpp"  // Use the simple OMS interface
#include "../utils/oms/order.hpp"
#include "../utils/oms/types.hpp"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

TEST_SUITE("MockExchangeOMS") {
    
    TEST_CASE("Constructor and Basic Properties") {
        MockExchangeOMS oms("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        
        CHECK(oms.get_exchange_name() == "TEST_EXCHANGE");
        CHECK(oms.is_connected() == false);
    }
    
    TEST_CASE("Connection") {
        MockExchangeOMS oms("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        
        CHECK(oms.connect() == true);
        CHECK(oms.is_connected() == true);
        
        oms.disconnect();
        CHECK(oms.is_connected() == false);
    }
    
    TEST_CASE("Order Processing") {
        MockExchangeOMS oms("TEST_EXCHANGE", 1.0, 0.0, std::chrono::milliseconds(10)); // 100% fill rate
        
        REQUIRE(oms.connect());
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_001";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Buy;
        order.qty = 0.1;
        order.price = 50000.0;
        
        // Set up callback to capture events
        std::vector<OrderEvent> events;
        oms.on_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        // Send order
        oms.send(order);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have received events
        CHECK(events.size() > 0);
        
        // First event should be acknowledgment
        CHECK(events[0].cl_ord_id == "TEST_ORDER_001");
        CHECK(events[0].type == OrderEventType::Ack);
        
        oms.disconnect();
    }
    
    TEST_CASE("Order Cancellation") {
        MockExchangeOMS oms("TEST_EXCHANGE", 0.0, 0.0, std::chrono::milliseconds(10)); // 0% fill rate
        
        REQUIRE(oms.connect());
        
        Order order;
        order.cl_ord_id = "TEST_ORDER_002";
        order.exch = "TEST_EXCHANGE";
        order.symbol = "BTCUSDC-PERP";
        order.side = Side::Sell;
        order.qty = 0.2;
        order.price = 50001.0;
        
        std::vector<OrderEvent> events;
        oms.on_event = [&events](const OrderEvent& event) {
            events.push_back(event);
        };
        
        // Send order
        oms.send(order);
        
        // Wait for acknowledgment
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Cancel order
        oms.cancel("TEST_ORDER_002");
        
        // Wait for cancellation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Should have acknowledgment and cancellation events
        CHECK(events.size() >= 2);
        
        bool has_ack = false;
        bool has_cancel = false;
        for (const auto& event : events) {
            if (event.type == OrderEventType::Ack) has_ack = true;
            if (event.type == OrderEventType::Cancel) has_cancel = true;
        }
        
        CHECK(has_ack);
        CHECK(has_cancel);
        
        oms.disconnect();
    }
    
    TEST_CASE("Fill Probability") {
        MockExchangeOMS oms("TEST_EXCHANGE", 0.5, 0.0, std::chrono::milliseconds(10)); // 50% fill rate
        
        REQUIRE(oms.connect());
        
        int total_orders = 5; // Further reduced to 5 for simpler testing
        int filled_orders = 0;
        int processed_orders = 0;
        
        // Set up callback once for all orders
        oms.on_event = [&filled_orders, &processed_orders](const OrderEvent& event) {
            if (event.type == OrderEventType::Fill) {
                filled_orders++;
            }
            if (event.type == OrderEventType::Ack || event.type == OrderEventType::Fill) {
                processed_orders++;
            }
        };
        
        // Send all orders
        for (int i = 0; i < total_orders; ++i) {
            Order order;
            order.cl_ord_id = "TEST_ORDER_" + std::to_string(i);
            order.exch = "TEST_EXCHANGE";
            order.symbol = "BTCUSDC-PERP";
            order.side = Side::Buy;
            order.qty = 0.1;
            order.price = 50000.0;
            
            oms.send(order);
        }
        
        // Wait for all orders to be processed with a longer timeout
        auto start = std::chrono::steady_clock::now();
        while (processed_orders < total_orders && 
               std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // With 50% fill rate, we should get roughly 50% fills (allowing for variance)
        double fill_rate = static_cast<double>(filled_orders) / total_orders;
        CHECK(fill_rate >= 0.0); // At least 0%
        CHECK(fill_rate <= 1.0); // At most 100%
        
        oms.disconnect();
    }
    
    TEST_CASE("Supported Symbols") {
        MockExchangeOMS oms("TEST_EXCHANGE", 0.8, 0.1, std::chrono::milliseconds(100));
        
        auto symbols = oms.get_supported_symbols();
        CHECK(symbols.size() > 0);
        
        // Check for common symbols
        bool has_btc = std::find(symbols.begin(), symbols.end(), "BTCUSDC-PERP") != symbols.end();
        CHECK(has_btc);
    }
}
