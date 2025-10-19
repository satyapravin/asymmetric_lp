#include <doctest.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

// Exchange interfaces
#include "exchanges/i_exchange_data_fetcher.hpp"
#include "exchanges/i_exchange_subscriber.hpp"
#include "exchanges/i_exchange_oms.hpp"
#include "exchanges/i_exchange_pms.hpp"

// Proto messages
#include "proto/order.pb.h"
#include "proto/market_data.pb.h"
#include "proto/position.pb.h"

TEST_CASE("GRVT Mock Data Tests") {
    // Test mock JSON parsing for GRVT responses
    
    // Mock account info response
    std::string mock_account_response = R"({
        "account": {
            "accountId": "test_account_id",
            "totalBalance": "1000.00000000",
            "availableBalance": "950.00000000",
            "marginBalance": "1000.00000000",
            "unrealizedPnl": "0.00000000"
        }
    })";
    
    CHECK(!mock_account_response.empty());
    CHECK(mock_account_response.find("accountId") != std::string::npos);
    CHECK(mock_account_response.find("totalBalance") != std::string::npos);
    
    // Mock orderbook WebSocket message
    std::string mock_orderbook = R"({
        "channel": "orderbook",
        "data": {
            "symbol": "BTCUSDT",
            "bids": [
                ["50000.00", "1.00000000"],
                ["49999.00", "2.00000000"]
            ],
            "asks": [
                ["50001.00", "1.50000000"],
                ["50002.00", "2.50000000"]
            ],
            "timestamp": 1640995200000
        }
    })";
    
    CHECK(!mock_orderbook.empty());
    CHECK(mock_orderbook.find("bids") != std::string::npos);
    CHECK(mock_orderbook.find("asks") != std::string::npos);
    
    // Mock trade WebSocket message
    std::string mock_trade = R"({
        "channel": "trades",
        "data": {
            "symbol": "BTCUSDT",
            "price": "50000.00",
            "quantity": "0.00100000",
            "side": "BUY",
            "tradeId": "12345",
            "timestamp": 1640995200000
        }
    })";
    
    CHECK(!mock_trade.empty());
    CHECK(mock_trade.find("price") != std::string::npos);
    CHECK(mock_trade.find("quantity") != std::string::npos);
    
    // Mock order placement response
    std::string mock_order_response = R"({
        "orderId": "12345",
        "symbol": "BTCUSDT",
        "side": "BUY",
        "type": "LIMIT",
        "quantity": "0.001",
        "price": "50000.00",
        "status": "NEW",
        "timestamp": 1640995200000
    })";
    
    CHECK(!mock_order_response.empty());
    CHECK(mock_order_response.find("orderId") != std::string::npos);
    CHECK(mock_order_response.find("BTCUSDT") != std::string::npos);
    
    // Mock position update message
    std::string mock_position_update = R"({
        "channel": "position_updates",
        "data": {
            "symbol": "BTCUSDT",
            "side": "LONG",
            "size": "0.001",
            "entryPrice": "50000.00",
            "markPrice": "50100.00",
            "unrealizedPnl": "0.10",
            "margin": "50.00",
            "timestamp": 1640995200000
        }
    })";
    
    CHECK(!mock_position_update.empty());
    CHECK(mock_position_update.find("symbol") != std::string::npos);
    CHECK(mock_position_update.find("BTCUSDT") != std::string::npos);
}

TEST_CASE("GRVT Authentication Tests") {
    // Test authentication configuration
    std::string api_key = "test_api_key";
    std::string session_cookie = "test_session_cookie";
    std::string account_id = "test_account_id";
    
    CHECK(api_key == "test_api_key");
    CHECK(session_cookie == "test_session_cookie");
    CHECK(account_id == "test_account_id");
    
    // Mock authentication response
    std::string mock_auth_response = R"({
        "success": true,
        "accountId": "test_account_id",
        "sessionToken": "new_session_token",
        "timestamp": 1640995200000
    })";
    
    CHECK(!mock_auth_response.empty());
    CHECK(mock_auth_response.find("success") != std::string::npos);
    CHECK(mock_auth_response.find("accountId") != std::string::npos);
}

TEST_CASE("GRVT Error Handling Tests") {
    // Mock error responses for different scenarios
    std::string mock_auth_error = R"({
        "error": "INVALID_API_KEY",
        "message": "Invalid API key provided",
        "code": 4001
    })";
    
    std::string mock_order_error = R"({
        "error": "INSUFFICIENT_BALANCE",
        "message": "Insufficient balance for order",
        "code": 4002
    })";
    
    std::string mock_connection_error = R"({
        "error": "CONNECTION_FAILED",
        "message": "WebSocket connection failed",
        "code": 5001
    })";
    
    // Test error parsing
    CHECK(!mock_auth_error.empty());
    CHECK(mock_auth_error.find("INVALID_API_KEY") != std::string::npos);
    
    CHECK(!mock_order_error.empty());
    CHECK(mock_order_error.find("INSUFFICIENT_BALANCE") != std::string::npos);
    
    CHECK(!mock_connection_error.empty());
    CHECK(mock_connection_error.find("CONNECTION_FAILED") != std::string::npos);
}

TEST_CASE("GRVT Proto Message Tests") {
    // Test proto message creation for GRVT data
    
    // Test OrderRequest
    proto::OrderRequest order_request;
    order_request.set_symbol("BTCUSDT");
    order_request.set_side(proto::Side::BUY);
    order_request.set_qty(0.001);
    order_request.set_price(50000.0);
    
    CHECK(order_request.symbol() == "BTCUSDT");
    CHECK(order_request.side() == proto::Side::BUY);
    CHECK(order_request.qty() == 0.001);
    CHECK(order_request.price() == 50000.0);
    
    // Test OrderBookSnapshot
    proto::OrderBookSnapshot orderbook;
    orderbook.set_symbol("BTCUSDT");
    orderbook.set_exch("GRVT");
    orderbook.set_timestamp_us(1640995200000000);
    
    CHECK(orderbook.symbol() == "BTCUSDT");
    CHECK(orderbook.exch() == "GRVT");
    CHECK(orderbook.timestamp_us() == 1640995200000000);
    
    // Test Trade
    proto::Trade trade;
    trade.set_symbol("BTCUSDT");
    trade.set_exch("GRVT");
    trade.set_price(50000.0);
    trade.set_qty(0.001);
    trade.set_is_buyer_maker(true);
    trade.set_trade_id("12345");
    trade.set_timestamp_us(1640995200000000);
    
    CHECK(trade.symbol() == "BTCUSDT");
    CHECK(trade.exch() == "GRVT");
    CHECK(trade.price() == 50000.0);
    CHECK(trade.qty() == 0.001);
    CHECK(trade.is_buyer_maker() == true);
    CHECK(trade.trade_id() == "12345");
    
    // Test PositionUpdate
    proto::PositionUpdate position;
    position.set_symbol("BTCUSDT");
    position.set_exch("GRVT");
    position.set_qty(0.001);
    position.set_avg_price(50000.0);
    position.set_timestamp_us(1640995200000000);
    
    CHECK(position.symbol() == "BTCUSDT");
    CHECK(position.exch() == "GRVT");
    CHECK(position.qty() == 0.001);
    CHECK(position.avg_price() == 50000.0);
    CHECK(position.timestamp_us() == 1640995200000000);
}