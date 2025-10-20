// Minimal order flow integration: Mock WS (Binance order events) -> TradingEngineLib -> ZMQ -> TraderLib ZmqOMSAdapter -> Strategy
#include "doctest.h"
#include "../tests/mocks/mock_websocket_transport.hpp"
#include "../../trader/trader_lib.hpp"
#include "../../trading_engine/trading_engine_lib.hpp"
#include "../../trader/zmq_oms_adapter.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"
#include "../../utils/zmq/zmq_subscriber.hpp"
#include "../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../proto/order.pb.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>

class OrderCaptureStrategy : public AbstractStrategy {
public:
    std::atomic<int> order_event_count{0};
    proto::OrderEvent last_event;

    OrderCaptureStrategy() : AbstractStrategy("OrderCaptureStrategy") {}

    void start() override { running_.store(true); }
    void stop() override { running_.store(false); }

    void on_order_event(const proto::OrderEvent& order_event) override {
        order_event_count++;
        last_event = order_event;
    }

    // Unused in this test
    void on_market_data(const proto::OrderBookSnapshot&) override {}
    void on_position_update(const proto::PositionUpdate&) override {}
    void on_trade_execution(const proto::Trade&) override {}
    void on_account_balance_update(const proto::AccountBalanceUpdate&) override {}

    // Query interface not used in this test
    std::optional<trader::PositionInfo> get_position(const std::string&, const std::string&) const override { return std::nullopt; }
    std::vector<trader::PositionInfo> get_all_positions() const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string&) const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string&) const override { return {}; }
    std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string&, const std::string&) const override { return std::nullopt; }
    std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string&) const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string&) const override { return {}; }
};

TEST_CASE("ORDER FLOW INTEGRATION TEST") {
    std::cout << "\n=== ORDER FLOW INTEGRATION TEST ===" << std::endl;
    // Endpoints consistent with TraderLib defaults
    const std::string oms_events_endpoint = "tcp://127.0.0.1:5558"; // TraderLib subscribes here
    const std::string engine_pub_endpoint = oms_events_endpoint;      // Engine publishes here
    const std::string engine_topic = "order_events";                // TraderLib default event topic

    // 1) Create TraderLib with strategy
    auto trader_lib = std::make_unique<trader::TraderLib>();
    trader_lib->set_exchange("binance");
    trader_lib->initialize("../tests/test_config.ini");
    auto strategy = std::make_shared<OrderCaptureStrategy>();
    trader_lib->set_strategy(strategy);
    trader_lib->start();

    // 2) Create TradingEngineLib and inject mock websocket
    auto engine = std::make_unique<trading_engine::TradingEngineLib>();
    engine->set_exchange("binance");
    engine->initialize("../tests/test_config.ini");
    // Configure engine publisher to TraderLib's expected endpoint
    auto engine_pub = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5558");
    engine->set_zmq_publisher(engine_pub);
    auto mock_ws = std::make_shared<test_utils::MockWebSocketTransport>();
    mock_ws->set_test_data_directory("data/binance/websocket");
    engine->set_websocket_transport(mock_ws);

    // 3) Start engine and replay an executionReport message via mock WS
    engine->start();
    mock_ws->start_event_loop();
    
    // Give ZMQ pub-sub connection time to fully establish (ZMQ "slow joiner" problem)
    // The subscriber connected before the publisher bound, so we need to wait for the connection to stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    mock_ws->load_and_replay_json_file("../tests/data/binance/websocket/order_update_message_ack.json");

    // 4) Wait for TraderLib's ZmqOMSAdapter to receive and forward to strategy
    int attempts = 0;
    while (strategy->order_event_count.load() == 0 && attempts < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }

    // 5) Assertions
    CHECK(strategy->order_event_count.load() > 0);
    CHECK(strategy->last_event.cl_ord_id() == "TEST_ORDER_1");
    CHECK(strategy->last_event.symbol() == "BTCUSDT");
    CHECK(strategy->last_event.exch() == "binance");

    // Cleanup
    trader_lib->stop();
    mock_ws->stop_event_loop();
    engine->stop();
}

