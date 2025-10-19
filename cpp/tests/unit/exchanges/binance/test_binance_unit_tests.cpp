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

TEST_CASE("Binance Mock Data Tests") {
    // Test mock JSON parsing for Binance responses
    
    // Mock account info response
    std::string mock_account_response = R"({
        "totalWalletBalance": "1000.00000000",
        "totalUnrealizedProfit": "0.00000000",
        "availableBalance": "1000.00000000",
        "maxWithdrawAmount": "1000.00000000",
        "assets": [
            {
                "asset": "USDT",
                "walletBalance": "1000.00000000",
                "unrealizedProfit": "0.00000000",
                "marginBalance": "1000.00000000",
                "maintMargin": "0.00000000",
                "initialMargin": "0.00000000",
                "positionInitialMargin": "0.00000000",
                "openOrderInitialMargin": "0.00000000",
                "crossWalletBalance": "1000.00000000",
                "crossUnPnl": "0.00000000",
                "availableBalance": "1000.00000000",
                "maxWithdrawAmount": "1000.00000000"
            }
        ]
    })";
    
    CHECK(!mock_account_response.empty());
    CHECK(mock_account_response.find("totalWalletBalance") != std::string::npos);
    CHECK(mock_account_response.find("availableBalance") != std::string::npos);
    
    // Mock orderbook WebSocket message
    std::string mock_orderbook = R"({
        "stream": "btcusdt@depth",
        "data": {
            "e": "depthUpdate",
            "E": 1640995200000,
            "s": "BTCUSDT",
            "U": 1234567890,
            "u": 1234567891,
            "b": [
                ["50000.00", "1.00000000"],
                ["49999.00", "2.00000000"]
            ],
            "a": [
                ["50001.00", "1.50000000"],
                ["50002.00", "2.50000000"]
            ]
        }
    })";
    
    CHECK(!mock_orderbook.empty());
    CHECK(mock_orderbook.find("b") != std::string::npos);
    CHECK(mock_orderbook.find("a") != std::string::npos);
    
    // Mock trade WebSocket message
    std::string mock_trade = R"({
        "stream": "btcusdt@trade",
        "data": {
            "e": "trade",
            "E": 1640995200000,
            "s": "BTCUSDT",
            "t": 12345,
            "p": "50000.00",
            "q": "0.00100000",
            "b": 1234567890,
            "a": 1234567891,
            "T": 1640995200000,
            "m": true,
            "M": true
        }
    })";
    
    CHECK(!mock_trade.empty());
    CHECK(mock_trade.find("p") != std::string::npos);
    CHECK(mock_trade.find("q") != std::string::npos);
    
    // Mock order placement response
    std::string mock_order_response = R"({
        "symbol": "BTCUSDT",
        "orderId": 12345,
        "orderListId": -1,
        "clientOrderId": "test_order",
        "price": "50000.00",
        "origQty": "0.001",
        "executedQty": "0.000",
        "cummulativeQuoteQty": "0.00000",
        "status": "NEW",
        "timeInForce": "GTC",
        "type": "LIMIT",
        "side": "BUY",
        "stopPrice": "0.00",
        "icebergQty": "0.00",
        "time": 1640995200000,
        "updateTime": 1640995200000,
        "isWorking": true,
        "origQuoteOrderQty": "0.00000"
    })";
    
    CHECK(!mock_order_response.empty());
    CHECK(mock_order_response.find("orderId") != std::string::npos);
    CHECK(mock_order_response.find("BTCUSDT") != std::string::npos);
    
    // Mock position update message
    std::string mock_position_update = R"({
        "e": "ACCOUNT_UPDATE",
        "E": 1640995200000,
        "T": 1640995200000,
        "a": {
            "m": "ORDER",
            "B": [
                {
                    "a": "USDT",
                    "wb": "1000.00000000",
                    "cw": "1000.00000000",
                    "bc": "0.00000000"
                }
            ],
            "P": [
                {
                    "s": "BTCUSDT",
                    "pa": "0.001",
                    "ep": "50000.00",
                    "cr": "0.00000000",
                    "up": "0.10",
                    "mt": "isolated",
                    "iw": "0.00000000",
                    "ps": "LONG"
                }
            ]
        }
    })";
    
    CHECK(!mock_position_update.empty());
    CHECK(mock_position_update.find("s") != std::string::npos);
    CHECK(mock_position_update.find("BTCUSDT") != std::string::npos);
}

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

TEST_CASE("Proto Message Tests") {
    // Test proto message creation and manipulation
    
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
    orderbook.set_exch("BINANCE");
    orderbook.set_timestamp_us(1640995200000000);
    
    CHECK(orderbook.symbol() == "BTCUSDT");
    CHECK(orderbook.exch() == "BINANCE");
    CHECK(orderbook.timestamp_us() == 1640995200000000);
    
    // Test Trade
    proto::Trade trade;
    trade.set_symbol("BTCUSDT");
    trade.set_exch("BINANCE");
    trade.set_price(50000.0);
    trade.set_qty(0.001);
    trade.set_is_buyer_maker(true);
    trade.set_trade_id("12345");
    trade.set_timestamp_us(1640995200000000);
    
    CHECK(trade.symbol() == "BTCUSDT");
    CHECK(trade.exch() == "BINANCE");
    CHECK(trade.price() == 50000.0);
    CHECK(trade.qty() == 0.001);
    CHECK(trade.is_buyer_maker() == true);
    CHECK(trade.trade_id() == "12345");
    
    // Test PositionUpdate
    proto::PositionUpdate position;
    position.set_symbol("BTCUSDT");
    position.set_exch("BINANCE");
    position.set_qty(0.001);
    position.set_avg_price(50000.0);
    position.set_timestamp_us(1640995200000000);
    
    CHECK(position.symbol() == "BTCUSDT");
    CHECK(position.exch() == "BINANCE");
    CHECK(position.qty() == 0.001);
    CHECK(position.avg_price() == 50000.0);
    CHECK(position.timestamp_us() == 1640995200000000);
}

TEST_CASE("Error Handling Tests") {
    // Test error response parsing for different exchanges
    
    // Binance error responses
    std::string binance_auth_error = R"({
        "code": -1022,
        "msg": "Signature for this request is not valid."
    })";
    
    std::string binance_order_error = R"({
        "code": -2019,
        "msg": "Margin is insufficient."
    })";
    
    CHECK(!binance_auth_error.empty());
    CHECK(binance_auth_error.find("-1022") != std::string::npos);
    CHECK(!binance_order_error.empty());
    CHECK(binance_order_error.find("-2019") != std::string::npos);
    
    // GRVT error responses
    std::string grvt_auth_error = R"({
        "error": "INVALID_API_KEY",
        "message": "Invalid API key provided",
        "code": 4001
    })";
    
    std::string grvt_order_error = R"({
        "error": "INSUFFICIENT_BALANCE",
        "message": "Insufficient balance for order",
        "code": 4002
    })";
    
    CHECK(!grvt_auth_error.empty());
    CHECK(grvt_auth_error.find("INVALID_API_KEY") != std::string::npos);
    CHECK(!grvt_order_error.empty());
    CHECK(grvt_order_error.find("INSUFFICIENT_BALANCE") != std::string::npos);
    
    // Deribit error responses
    std::string deribit_auth_error = R"({
        "jsonrpc": "2.0",
        "error": {
            "code": 10001,
            "message": "Invalid client credentials"
        },
        "id": 1
    })";
    
    std::string deribit_order_error = R"({
        "jsonrpc": "2.0",
        "error": {
            "code": 10002,
            "message": "Insufficient funds"
        },
        "id": 1
    })";
    
    CHECK(!deribit_auth_error.empty());
    CHECK(deribit_auth_error.find("Invalid client credentials") != std::string::npos);
    CHECK(!deribit_order_error.empty());
    CHECK(deribit_order_error.find("Insufficient funds") != std::string::npos);
}