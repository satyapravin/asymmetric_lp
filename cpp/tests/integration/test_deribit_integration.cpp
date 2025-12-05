#include "doctest.h"
#include "../../exchanges/deribit/public_websocket/deribit_subscriber.hpp"
#include "../../exchanges/deribit/private_websocket/deribit_oms.hpp"
#include "../../exchanges/deribit/private_websocket/deribit_pms.hpp"
#include "../../proto/market_data.pb.h"
#include "../../proto/order.pb.h"
#include "../../proto/position.pb.h"
#include "../mocks/mock_websocket_transport.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

namespace deribit_test {

// Helper function to read JSON file (tries multiple paths)
std::string read_json_file(const std::string& filepath) {
    std::vector<std::string> paths = {
        filepath,
        "../" + filepath,
        "../../" + filepath,
        "cpp/" + filepath,
        "../cpp/" + filepath,
        "tests/" + filepath,
        "../tests/" + filepath
    };
    
    for (const auto& path : paths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
    }
    
    throw std::runtime_error("Failed to open file: " + filepath + " (tried multiple paths)");
}

// Test strategy to capture callbacks
class DeribitTestStrategy {
public:
    std::atomic<int> orderbook_count{0};
    std::atomic<int> trade_count{0};
    std::atomic<int> order_event_count{0};
    std::atomic<int> position_update_count{0};
    std::atomic<int> balance_update_count{0};
    
    proto::OrderBookSnapshot last_orderbook;
    proto::Trade last_trade;
    proto::OrderEvent last_order_event;
    proto::PositionUpdate last_position;
    
    void on_orderbook(const proto::OrderBookSnapshot& orderbook) {
        orderbook_count++;
        last_orderbook = orderbook;
    }
    
    void on_trade(const proto::Trade& trade) {
        trade_count++;
        last_trade = trade;
    }
    
    void on_order_event(const proto::OrderEvent& event) {
        order_event_count++;
        last_order_event = event;
    }
    
    void on_position(const proto::PositionUpdate& position) {
        position_update_count++;
        last_position = position;
    }
};

} // namespace deribit_test

TEST_SUITE("Deribit Integration Tests") {

TEST_CASE("DeribitSubscriber - Subscription Message Format") {
    std::cout << "\n=== Deribit Subscription Message Test ===" << std::endl;
    
    deribit::DeribitSubscriberConfig config;
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    config.testnet = true;
    
    deribit::DeribitSubscriber subscriber(config);
    
    // Test orderbook subscription message
    std::string orderbook_sub = subscriber.create_subscription_message("BTC-PERPETUAL", "book", "raw");
    CHECK(orderbook_sub.find("public/subscribe") != std::string::npos);
    CHECK(orderbook_sub.find("book.BTC-PERPETUAL.raw") != std::string::npos);
    CHECK(orderbook_sub.find("jsonrpc") != std::string::npos);
    
    // Test trades subscription message
    std::string trades_sub = subscriber.create_subscription_message("BTC-PERPETUAL", "trades", "raw");
    CHECK(trades_sub.find("public/subscribe") != std::string::npos);
    CHECK(trades_sub.find("trades.BTC-PERPETUAL.raw") != std::string::npos);
    
    // Test with interval
    std::string orderbook_100ms = subscriber.create_subscription_message("BTC-PERPETUAL", "book", "100ms");
    CHECK(orderbook_100ms.find("book.BTC-PERPETUAL.100ms") != std::string::npos);
    
    std::cout << "[TEST] ✅ Subscription message format correct" << std::endl;
}

TEST_CASE("DeribitSubscriber - Orderbook Parsing") {
    std::cout << "\n=== Deribit Orderbook Parsing Test ===" << std::endl;
    
    deribit::DeribitSubscriberConfig config;
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitSubscriber subscriber(config);
    
    deribit_test::DeribitTestStrategy strategy;
    subscriber.set_orderbook_callback([&strategy](const proto::OrderBookSnapshot& orderbook) {
        strategy.on_orderbook(orderbook);
    });
    
    // Test orderbook update message
    std::string orderbook_msg = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "book.BTC-PERPETUAL.raw",
            "data": {
                "bids": [["50000.0", "0.1"], ["49999.0", "0.2"]],
                "asks": [["50001.0", "0.15"], ["50002.0", "0.25"]],
                "timestamp": 1640995200000,
                "change_id": 12345
            }
        }
    })";
    
    subscriber.handle_websocket_message(orderbook_msg);
    
    CHECK(strategy.orderbook_count.load() == 1);
    CHECK(strategy.last_orderbook.exch() == "DERIBIT");
    CHECK(strategy.last_orderbook.symbol() == "BTC-PERPETUAL");
    CHECK(strategy.last_orderbook.bids_size() == 2);
    CHECK(strategy.last_orderbook.asks_size() == 2);
    CHECK(strategy.last_orderbook.bids(0).price() == 50000.0);
    CHECK(strategy.last_orderbook.bids(0).qty() == 0.1);
    
    std::cout << "[TEST] ✅ Orderbook parsing successful" << std::endl;
}

TEST_CASE("DeribitSubscriber - Trade Parsing") {
    std::cout << "\n=== Deribit Trade Parsing Test ===" << std::endl;
    
    deribit::DeribitSubscriberConfig config;
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitSubscriber subscriber(config);
    
    deribit_test::DeribitTestStrategy strategy;
    subscriber.set_trade_callback([&strategy](const proto::Trade& trade) {
        strategy.on_trade(trade);
    });
    
    // Test trade update message (Deribit sends array of trades)
    std::string trade_msg = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "trades.BTC-PERPETUAL.raw",
            "data": [{
                "price": 50000.5,
                "amount": 0.1,
                "direction": "buy",
                "timestamp": 1640995200000,
                "trade_id": "trade_12345",
                "trade_seq": 12345
            }]
        }
    })";
    
    subscriber.handle_websocket_message(trade_msg);
    
    CHECK(strategy.trade_count.load() == 1);
    CHECK(strategy.last_trade.exch() == "DERIBIT");
    CHECK(strategy.last_trade.symbol() == "BTC-PERPETUAL");
    CHECK(strategy.last_trade.price() == 50000.5);
    CHECK(strategy.last_trade.qty() == 0.1);
    CHECK(strategy.last_trade.is_buyer_maker() == false); // direction="buy" means buyer is taker
    
    std::cout << "[TEST] ✅ Trade parsing successful" << std::endl;
}

TEST_CASE("DeribitOMS - Order Message Creation") {
    std::cout << "\n=== Deribit OMS Order Message Test ===" << std::endl;
    
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    // Test limit buy order
    std::string buy_order = oms.create_order_message("BTC-PERPETUAL", "BUY", 0.1, 50000.0, "LIMIT");
    CHECK(buy_order.find("private/buy") != std::string::npos);
    CHECK(buy_order.find("\"instrument_name\":\"BTC-PERPETUAL\"") != std::string::npos);
    CHECK(buy_order.find("\"amount\":0.1") != std::string::npos);
    CHECK(buy_order.find("\"price\":50000") != std::string::npos);
    CHECK(buy_order.find("\"type\":\"limit\"") != std::string::npos);
    
    // Test market sell order
    std::string sell_order = oms.create_order_message("BTC-PERPETUAL", "SELL", 0.2, 0.0, "MARKET");
    CHECK(sell_order.find("private/sell") != std::string::npos);
    CHECK(sell_order.find("\"type\":\"market\"") != std::string::npos);
    
    std::cout << "[TEST] ✅ Order message creation successful" << std::endl;
}

TEST_CASE("DeribitOMS - Order Event Handling") {
    std::cout << "\n=== Deribit OMS Order Event Test ===" << std::endl;
    
    deribit::DeribitOMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitOMS oms(config);
    
    deribit_test::DeribitTestStrategy strategy;
    oms.set_order_status_callback([&strategy](const proto::OrderEvent& event) {
        strategy.on_order_event(event);
    });
    
    // Test order update from subscription
    std::string order_update = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "user.orders.BTC-PERPETUAL.raw",
            "data": {
                "order_id": "order_12345",
                "order_state": "filled",
                "instrument_name": "BTC-PERPETUAL",
                "direction": "buy",
                "amount": 0.1,
                "price": 50000,
                "timestamp": 1640995200000
            }
        }
    })";
    
    oms.handle_websocket_message(order_update);
    
    CHECK(strategy.order_event_count.load() == 1);
    CHECK(strategy.last_order_event.exch_order_id() == "order_12345");
    CHECK(strategy.last_order_event.symbol() == "BTC-PERPETUAL");
    CHECK(strategy.last_order_event.event_type() == proto::OrderEventType::FILL);
    CHECK(strategy.last_order_event.fill_qty() == 0.1);
    CHECK(strategy.last_order_event.fill_price() == 50000.0);
    
    std::cout << "[TEST] ✅ Order event handling successful" << std::endl;
}

TEST_CASE("DeribitPMS - Position Update Handling") {
    std::cout << "\n=== Deribit PMS Position Update Test ===" << std::endl;
    
    deribit::DeribitPMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    config.currency = "BTC";
    
    deribit::DeribitPMS pms(config);
    
    deribit_test::DeribitTestStrategy strategy;
    pms.set_position_update_callback([&strategy](const proto::PositionUpdate& position) {
        strategy.on_position(position);
    });
    
    // Test position update from portfolio subscription
    std::string position_update = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "user.portfolio.BTC",
            "data": {
                "instrument_name": "BTC-PERPETUAL",
                "size": 0.1,
                "average_price": 50000,
                "mark_price": 50100,
                "unrealized_pnl": 10.0,
                "timestamp": 1640995200000
            }
        }
    })";
    
    pms.handle_websocket_message(position_update);
    
    CHECK(strategy.position_update_count.load() == 1);
    CHECK(strategy.last_position.exch() == "DERIBIT");
    CHECK(strategy.last_position.symbol() == "BTC-PERPETUAL");
    CHECK(strategy.last_position.qty() == 0.1);
    CHECK(strategy.last_position.avg_price() == 50000.0);
    
    std::cout << "[TEST] ✅ Position update handling successful" << std::endl;
}

TEST_CASE("DeribitPMS - Balance Update Handling") {
    std::cout << "\n=== Deribit PMS Balance Update Test ===" << std::endl;
    
    deribit::DeribitPMSConfig config;
    config.client_id = "test_client_id";
    config.client_secret = "test_client_secret";
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    config.currency = "BTC";
    
    deribit::DeribitPMS pms(config);
    
    std::atomic<int> balance_update_count{0};
    proto::AccountBalanceUpdate last_balance_update;
    
    pms.set_account_balance_update_callback([&balance_update_count, &last_balance_update](const proto::AccountBalanceUpdate& update) {
        balance_update_count++;
        last_balance_update = update;
    });
    
    // Test balance update from portfolio channel
    std::string balance_update = R"({
        "jsonrpc": "2.0",
        "method": "subscription",
        "params": {
            "channel": "user.changes.any.any",
            "data": {
                "portfolio": {
                    "BTC": {
                        "balance": 1.5,
                        "available": 1.0,
                        "locked": 0.5
                    },
                    "USDT": {
                        "balance": 10000.0,
                        "available": 9500.0,
                        "locked": 500.0
                    }
                }
            }
        }
    })";
    
    pms.handle_websocket_message(balance_update);
    
    CHECK(balance_update_count.load() == 1);
    CHECK(last_balance_update.balances_size() == 2);
    
    // Find BTC balance
    bool found_btc = false;
    bool found_usdt = false;
    for (int i = 0; i < last_balance_update.balances_size(); i++) {
        const auto& balance = last_balance_update.balances(i);
        if (balance.instrument() == "BTC") {
            found_btc = true;
            CHECK(balance.balance() == 1.5);
            CHECK(balance.available() == 1.0);
            CHECK(balance.locked() == 0.5);
        } else if (balance.instrument() == "USDT") {
            found_usdt = true;
            CHECK(balance.balance() == 10000.0);
            CHECK(balance.available() == 9500.0);
            CHECK(balance.locked() == 500.0);
        }
    }
    
    CHECK(found_btc == true);
    CHECK(found_usdt == true);
    
    std::cout << "[TEST] ✅ Balance update handling successful" << std::endl;
}

TEST_CASE("DeribitSubscriber - Mock WebSocket Integration") {
    std::cout << "\n=== Deribit Mock WebSocket Integration Test ===" << std::endl;
    
    deribit::DeribitSubscriberConfig config;
    config.websocket_url = "wss://test.deribit.com/ws/api/v2";
    
    deribit::DeribitSubscriber subscriber(config);
    
    // Create mock WebSocket transport
    auto mock_transport = std::make_unique<test_utils::MockWebSocketTransport>();
    mock_transport->set_test_data_directory("data/deribit/websocket");
    
    subscriber.set_websocket_transport(std::move(mock_transport));
    
    deribit_test::DeribitTestStrategy strategy;
    subscriber.set_orderbook_callback([&strategy](const proto::OrderBookSnapshot& orderbook) {
        strategy.on_orderbook(orderbook);
    });
    subscriber.set_trade_callback([&strategy](const proto::Trade& trade) {
        strategy.on_trade(trade);
    });
    
    // Connect and subscribe
    CHECK(subscriber.connect() == true);
    CHECK(subscriber.subscribe_orderbook("BTC-PERPETUAL", 20, 100) == true);
    CHECK(subscriber.subscribe_trades("BTC-PERPETUAL") == true);
    
    subscriber.start();
    
    // Give time for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop subscriber (this will disconnect and stop the websocket thread)
    subscriber.stop();
    
    // Give threads time to clean up
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "[TEST] ✅ Mock WebSocket integration successful" << std::endl;
}

} // TEST_SUITE

