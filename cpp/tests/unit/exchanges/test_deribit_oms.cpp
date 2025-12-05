#include "doctest.h"
#include "../../../exchanges/deribit/private_websocket/deribit_oms.hpp"
#include "../../../proto/order.pb.h"
#include <memory>

TEST_CASE("DeribitOMS - Initialization") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    config.testnet = true;
    
    deribit::DeribitOMS oms(config);
    
    // Test that OMS is initialized
    CHECK(oms.is_authenticated() == true); // Should be authenticated when credentials are provided
    CHECK_NOTHROW(oms.is_connected()); // Should not crash
}

TEST_CASE("DeribitOMS - Authentication") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    // Test authentication with credentials
    oms.set_auth_credentials("test_client_id", "test_client_secret");
    CHECK(oms.is_authenticated() == true);
    
    // Test authentication without credentials
    deribit::DeribitOMS oms2(config);
    oms2.set_auth_credentials("", "");
    CHECK(oms2.is_authenticated() == false);
}

TEST_CASE("DeribitOMS - Create Order Message") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    // Test limit buy order message
    std::string buy_msg = oms.create_order_message("BTC-PERPETUAL", "BUY", 0.1, 50000.0, "LIMIT");
    CHECK(buy_msg.find("private/buy") != std::string::npos);
    CHECK(buy_msg.find("BTC-PERPETUAL") != std::string::npos);
    CHECK(buy_msg.find("0.1") != std::string::npos);
    CHECK(buy_msg.find("50000") != std::string::npos);
    CHECK(buy_msg.find("limit") != std::string::npos);
    
    // Test limit sell order message
    std::string sell_msg = oms.create_order_message("BTC-PERPETUAL", "SELL", 0.2, 51000.0, "LIMIT");
    CHECK(sell_msg.find("private/sell") != std::string::npos);
    CHECK(sell_msg.find("BTC-PERPETUAL") != std::string::npos);
    CHECK(sell_msg.find("0.2") != std::string::npos);
    CHECK(sell_msg.find("51000") != std::string::npos);
    
    // Test market order message
    std::string market_msg = oms.create_order_message("BTC-PERPETUAL", "BUY", 0.1, 0.0, "MARKET");
    CHECK(market_msg.find("market") != std::string::npos);
}

TEST_CASE("DeribitOMS - Create Cancel Message") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    std::string cancel_msg = oms.create_cancel_message("CL_ORDER_1", "EXCH_ORDER_123");
    CHECK(cancel_msg.find("private/cancel") != std::string::npos);
    CHECK(cancel_msg.find("EXCH_ORDER_123") != std::string::npos);
}

TEST_CASE("DeribitOMS - Handle Order Update") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    // Test order update message
    std::string order_update_json = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "user.orders.BTC-PERPETUAL.raw",
            "data": {
                "order_id": "12345",
                "order_state": "filled",
                "instrument_name": "BTC-PERPETUAL",
                "direction": "buy",
                "amount": 0.1,
                "price": 50000,
                "timestamp": 1640995200000
            }
        }
    })";
    
    bool callback_called = false;
    proto::OrderEvent last_event;
    
    oms.set_order_status_callback([&callback_called, &last_event](const proto::OrderEvent& event) {
        callback_called = true;
        last_event = event;
    });
    
    oms.handle_websocket_message(order_update_json);
    
    CHECK(callback_called == true);
    CHECK(last_event.exch_order_id() == "12345");
    CHECK(last_event.symbol() == "BTC-PERPETUAL");
    CHECK(last_event.event_type() == proto::OrderEventType::FILL);
    CHECK(last_event.fill_qty() == 0.1);
    CHECK(last_event.fill_price() == 50000.0);
}

TEST_CASE("DeribitOMS - Handle Authentication Response") {
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    std::string auth_response = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "result": {
            "access_token": "test_access_token_12345",
            "expires_in": 3600
        }
    })";
    
    oms.handle_websocket_message(auth_response);
    
    // Authentication should succeed (token stored in config)
    CHECK(oms.is_authenticated() == true);
}

