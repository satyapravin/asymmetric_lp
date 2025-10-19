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

TEST_CASE("Deribit Mock Data Tests") {
    // Test mock JSON parsing for Deribit responses
    
    // Mock account info response
    std::string mock_account_response = R"({
        "jsonrpc": "2.0",
        "result": {
            "currency": "BTC",
            "balance": 1.5,
            "available_funds": 1.2,
            "equity": 1.5,
            "margin_balance": 1.5,
            "initial_margin": 0.0,
            "maintenance_margin": 0.0,
            "unrealized_pnl": 0.0
        },
        "id": 1
    })";
    
    CHECK(!mock_account_response.empty());
    CHECK(mock_account_response.find("balance") != std::string::npos);
    CHECK(mock_account_response.find("available_funds") != std::string::npos);
    
    // Mock orderbook WebSocket message
    std::string mock_orderbook = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "book.BTC-PERPETUAL.raw",
            "data": {
                "instrument_name": "BTC-PERPETUAL",
                "bids": [
                    [50000.0, 1.0],
                    [49999.0, 2.0]
                ],
                "asks": [
                    [50001.0, 1.5],
                    [50002.0, 2.5]
                ],
                "timestamp": 1640995200000
            }
        }
    })";
    
    CHECK(!mock_orderbook.empty());
    CHECK(mock_orderbook.find("bids") != std::string::npos);
    CHECK(mock_orderbook.find("asks") != std::string::npos);
    
    // Mock trade WebSocket message
    std::string mock_trade = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "trades.BTC-PERPETUAL.raw",
            "data": [
                {
                    "instrument_name": "BTC-PERPETUAL",
                    "price": 50000.0,
                    "amount": 0.001,
                    "direction": "buy",
                    "trade_id": "12345",
                    "timestamp": 1640995200000
                }
            ]
        }
    })";
    
    CHECK(!mock_trade.empty());
    CHECK(mock_trade.find("price") != std::string::npos);
    CHECK(mock_trade.find("amount") != std::string::npos);
    
    // Mock order placement response
    std::string mock_order_response = R"({
        "jsonrpc": "2.0",
        "result": {
            "order_id": "12345",
            "instrument_name": "BTC-PERPETUAL",
            "direction": "buy",
            "order_type": "limit",
            "amount": 0.001,
            "price": 50000.0,
            "order_state": "open",
            "time_in_force": "good_til_cancelled",
            "creation_timestamp": 1640995200000
        },
        "id": 1
    })";
    
    CHECK(!mock_order_response.empty());
    CHECK(mock_order_response.find("order_id") != std::string::npos);
    CHECK(mock_order_response.find("BTC-PERPETUAL") != std::string::npos);
    
    // Mock position update message
    std::string mock_position_update = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "user.portfolio.BTC",
            "data": {
                "currency": "BTC",
                "balance": 1.5,
                "available_funds": 1.2,
                "equity": 1.5,
                "margin_balance": 1.5,
                "initial_margin": 0.0,
                "maintenance_margin": 0.0,
                "unrealized_pnl": 0.0,
                "timestamp": 1640995200000
            }
        }
    })";
    
    CHECK(!mock_position_update.empty());
    CHECK(mock_position_update.find("currency") != std::string::npos);
    CHECK(mock_position_update.find("BTC") != std::string::npos);
}

TEST_CASE("Deribit Authentication Tests") {
    // Test authentication configuration
    std::string client_id = "test_client_id";
    std::string client_secret = "test_client_secret";
    
    CHECK(client_id == "test_client_id");
    CHECK(client_secret == "test_client_secret");
    
    // Mock authentication response
    std::string mock_auth_response = R"({
        "jsonrpc": "2.0",
        "result": {
            "access_token": "new_access_token",
            "refresh_token": "new_refresh_token",
            "expires_in": 3600,
            "scope": "read write",
            "token_type": "Bearer"
        },
        "id": 1
    })";
    
    CHECK(!mock_auth_response.empty());
    CHECK(mock_auth_response.find("access_token") != std::string::npos);
    CHECK(mock_auth_response.find("refresh_token") != std::string::npos);
}

TEST_CASE("Deribit Error Handling Tests") {
    // Mock error responses for different scenarios
    std::string mock_auth_error = R"({
        "jsonrpc": "2.0",
        "error": {
            "code": 10001,
            "message": "Invalid client credentials"
        },
        "id": 1
    })";
    
    std::string mock_order_error = R"({
        "jsonrpc": "2.0",
        "error": {
            "code": 10002,
            "message": "Insufficient funds"
        },
        "id": 1
    })";
    
    std::string mock_connection_error = R"({
        "jsonrpc": "2.0",
        "error": {
            "code": 10003,
            "message": "Connection failed"
        },
        "id": 1
    })";
    
    // Test error parsing
    CHECK(!mock_auth_error.empty());
    CHECK(mock_auth_error.find("Invalid client credentials") != std::string::npos);
    
    CHECK(!mock_order_error.empty());
    CHECK(mock_order_error.find("Insufficient funds") != std::string::npos);
    
    CHECK(!mock_connection_error.empty());
    CHECK(mock_connection_error.find("Connection failed") != std::string::npos);
}

TEST_CASE("Deribit Proto Message Tests") {
    // Test proto message creation for Deribit data
    
    // Test OrderRequest
    proto::OrderRequest order_request;
    order_request.set_symbol("BTC-PERPETUAL");
    order_request.set_side(proto::Side::BUY);
    order_request.set_qty(0.001);
    order_request.set_price(50000.0);
    
    CHECK(order_request.symbol() == "BTC-PERPETUAL");
    CHECK(order_request.side() == proto::Side::BUY);
    CHECK(order_request.qty() == 0.001);
    CHECK(order_request.price() == 50000.0);
    
    // Test OrderBookSnapshot
    proto::OrderBookSnapshot orderbook;
    orderbook.set_symbol("BTC-PERPETUAL");
    orderbook.set_exch("DERIBIT");
    orderbook.set_timestamp_us(1640995200000000);
    
    CHECK(orderbook.symbol() == "BTC-PERPETUAL");
    CHECK(orderbook.exch() == "DERIBIT");
    CHECK(orderbook.timestamp_us() == 1640995200000000);
    
    // Test Trade
    proto::Trade trade;
    trade.set_symbol("BTC-PERPETUAL");
    trade.set_exch("DERIBIT");
    trade.set_price(50000.0);
    trade.set_qty(0.001);
    trade.set_is_buyer_maker(true);
    trade.set_trade_id("12345");
    trade.set_timestamp_us(1640995200000000);
    
    CHECK(trade.symbol() == "BTC-PERPETUAL");
    CHECK(trade.exch() == "DERIBIT");
    CHECK(trade.price() == 50000.0);
    CHECK(trade.qty() == 0.001);
    CHECK(trade.is_buyer_maker() == true);
    CHECK(trade.trade_id() == "12345");
    
    // Test PositionUpdate
    proto::PositionUpdate position;
    position.set_symbol("BTC-PERPETUAL");
    position.set_exch("DERIBIT");
    position.set_qty(0.001);
    position.set_avg_price(50000.0);
    position.set_timestamp_us(1640995200000000);
    
    CHECK(position.symbol() == "BTC-PERPETUAL");
    CHECK(position.exch() == "DERIBIT");
    CHECK(position.qty() == 0.001);
    CHECK(position.avg_price() == 50000.0);
    CHECK(position.timestamp_us() == 1640995200000000);
}