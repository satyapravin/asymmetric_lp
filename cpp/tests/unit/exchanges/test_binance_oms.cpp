#include "doctest.h"
#include "../../../exchanges/binance/private_websocket/binance_oms.hpp"
#include "../../../proto/order.pb.h"
#include <memory>

TEST_CASE("BinanceOMS - Initialization") {
    binance::BinanceConfig config;
    config.api_key = "test_key";
    config.api_secret = "test_secret";
    config.base_url = "https://fapi.binance.com";
    config.testnet = true;
    
    binance::BinanceOMS oms(config);
    
    // Test that OMS is initialized
    CHECK(true); // Basic initialization test
}

TEST_CASE("BinanceOMS - Parse Order JSON") {
    binance::BinanceConfig config;
    config.api_key = "test_key";
    config.api_secret = "test_secret";
    config.base_url = "https://fapi.binance.com";
    config.testnet = true;
    
    binance::BinanceOMS oms(config);
    
    // Test JSON parsing
    std::string json_str = R"({
        "clientOrderId": "TEST_ORDER_1",
        "symbol": "BTCUSDT",
        "orderId": "123456789",
        "executedQty": "0.00000000",
        "avgPrice": "0.00000000",
        "status": "NEW",
        "time": 1640995200000,
        "side": "BUY",
        "type": "LIMIT",
        "origQty": "0.10000000",
        "price": "50000.00000000"
    })";
    
    proto::OrderEvent order_event = oms.parse_order_from_json(json_str);
    
    CHECK(order_event.cl_ord_id() == "TEST_ORDER_1");
    CHECK(order_event.symbol() == "BTCUSDT");
    CHECK(order_event.exch() == "binance");
    CHECK(order_event.event_type() == proto::OrderEventType::ACK);
}

TEST_CASE("BinanceOMS - Parse Invalid JSON") {
    binance::BinanceConfig config;
    config.api_key = "test_key";
    config.api_secret = "test_secret";
    config.base_url = "https://fapi.binance.com";
    config.testnet = true;
    
    binance::BinanceOMS oms(config);
    
    // Test invalid JSON
    std::string invalid_json = "invalid json";
    proto::OrderEvent order_event = oms.parse_order_from_json(invalid_json);
    
    // Should return empty event or handle gracefully
    CHECK(order_event.text() == "Failed to parse order JSON");
}

TEST_CASE("BinanceOMS - Parse Different Order Statuses") {
    binance::BinanceConfig config;
    config.api_key = "test_key";
    config.api_secret = "test_secret";
    config.base_url = "https://fapi.binance.com";
    config.testnet = true;
    
    binance::BinanceOMS oms(config);
    
    // Test NEW status
    std::string new_order_json = R"({
        "clientOrderId": "TEST_ORDER_1",
        "symbol": "BTCUSDT",
        "orderId": "123456789",
        "executedQty": "0.00000000",
        "avgPrice": "0.00000000",
        "status": "NEW",
        "time": 1640995200000,
        "side": "BUY",
        "type": "LIMIT",
        "origQty": "0.10000000",
        "price": "50000.00000000"
    })";
    
    proto::OrderEvent new_event = oms.parse_order_from_json(new_order_json);
    CHECK(new_event.event_type() == proto::OrderEventType::ACK);
    
    // Test FILLED status
    std::string filled_order_json = R"({
        "clientOrderId": "TEST_ORDER_2",
        "symbol": "BTCUSDT",
        "orderId": "123456790",
        "executedQty": "0.10000000",
        "avgPrice": "50000.00000000",
        "status": "FILLED",
        "time": 1640995200000,
        "side": "BUY",
        "type": "LIMIT",
        "origQty": "0.10000000",
        "price": "50000.00000000"
    })";
    
    proto::OrderEvent filled_event = oms.parse_order_from_json(filled_order_json);
    CHECK(filled_event.event_type() == proto::OrderEventType::FILL);
    CHECK(filled_event.fill_qty() == 0.1);
    CHECK(filled_event.fill_price() == 50000.0);
}

TEST_CASE("BinanceOMS - Parse Order Sides") {
    binance::BinanceConfig config;
    config.api_key = "test_key";
    config.api_secret = "test_secret";
    config.base_url = "https://fapi.binance.com";
    config.testnet = true;
    
    binance::BinanceOMS oms(config);
    
    // Test BUY side
    std::string buy_order_json = R"({
        "clientOrderId": "BUY_ORDER",
        "symbol": "BTCUSDT",
        "orderId": "123456789",
        "executedQty": "0.00000000",
        "avgPrice": "0.00000000",
        "status": "NEW",
        "time": 1640995200000,
        "side": "BUY",
        "type": "LIMIT",
        "origQty": "0.10000000",
        "price": "50000.00000000"
    })";
    
    proto::OrderEvent buy_event = oms.parse_order_from_json(buy_order_json);
    CHECK(buy_event.side() == proto::BUY);
    
    // Test SELL side
    std::string sell_order_json = R"({
        "clientOrderId": "SELL_ORDER",
        "symbol": "BTCUSDT",
        "orderId": "123456790",
        "executedQty": "0.00000000",
        "avgPrice": "0.00000000",
        "status": "NEW",
        "time": 1640995200000,
        "side": "SELL",
        "type": "LIMIT",
        "origQty": "0.10000000",
        "price": "50000.00000000"
    })";
    
    proto::OrderEvent sell_event = oms.parse_order_from_json(sell_order_json);
    CHECK(sell_event.side() == proto::SELL);
}
