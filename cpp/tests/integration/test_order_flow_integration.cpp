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
#include "../../proto/acc_balance.pb.h"
#include "../../proto/position.pb.h"
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
    
    // Use unique config with ports 6300-6304 to avoid conflicts with other tests
    const std::string config_file = "../../tests/config/test_config_orderflow.ini";
    
    // Endpoints consistent with config
    const std::string oms_events_endpoint = "tcp://127.0.0.1:6303"; // TraderLib subscribes here
    const std::string engine_pub_endpoint = oms_events_endpoint;      // Engine publishes here
    const std::string engine_topic = "order_events";                // TraderLib default event topic

    // 1) Create TradingEngineLib and inject mock websocket FIRST (publisher binds before subscriber connects)
    auto engine = std::make_unique<trading_engine::TradingEngineLib>();
    engine->set_exchange("binance");
    engine->initialize(config_file);
    // Configure engine publisher to TraderLib's expected endpoint
    auto engine_pub = std::make_shared<ZmqPublisher>(oms_events_endpoint);
    engine->set_zmq_publisher(engine_pub);
    auto mock_ws = std::make_shared<test_utils::MockWebSocketTransport>();
    mock_ws->set_test_data_directory("data/binance/websocket");
    engine->set_websocket_transport(mock_ws);

    // 2) Start engine (publisher binds) and give it time to be ready
    engine->start();
    mock_ws->start_event_loop();
    
    // Give publisher time to bind and be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 3) Create TraderLib with strategy AFTER publisher is bound (subscriber connects after publisher binds)
    auto trader_lib = std::make_unique<trader::TraderLib>();
    trader_lib->set_exchange("binance");
    trader_lib->initialize(config_file);
    
    // Give subscriber time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto strategy = std::make_shared<OrderCaptureStrategy>();
    trader_lib->set_strategy(strategy);
    trader_lib->start();
    
    // Send mock balance, position, and order events to allow strategy to start
    proto::AccountBalanceUpdate balance_update;
    auto* balance = balance_update.add_balances();
    balance->set_exch("binance");
    balance->set_instrument("USDT");
    balance->set_available(10000.0);
    balance->set_locked(0.0);
    trader_lib->simulate_balance_update(balance_update);
    
    proto::PositionUpdate position_update;
    position_update.set_exch("binance");
    position_update.set_symbol("BTCUSDT");
    position_update.set_qty(0.0);
    trader_lib->simulate_position_update(position_update);
    
    proto::OrderEvent init_order_event;
    init_order_event.set_cl_ord_id("init_order");
    init_order_event.set_exch("binance");
    init_order_event.set_symbol("BTCUSDT");
    init_order_event.set_event_type(proto::ACK);
    trader_lib->simulate_order_event(init_order_event);
    
    // Give strategy container time to process and start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 4) Give ZMQ pub-sub connection time to fully establish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 5) Replay an executionReport message via mock WS
    std::cout << "[TEST] Loading and replaying order update message..." << std::endl;
    std::cout << "[TEST] Strategy order_event_count before replay: " << strategy->order_event_count.load() << std::endl;
    mock_ws->load_and_replay_json_file("../tests/data/binance/websocket/order_update_message_ack.json");

    // 6) Wait for TraderLib's ZmqOMSAdapter to receive and forward to strategy
    // Need to wait for: MockWS -> BinanceOMS -> TradingEngine -> ZMQ -> TraderLib -> Strategy
    std::cout << "[TEST] Waiting for order event to propagate through the chain..." << std::endl;
    int attempts = 0;
    int expected_count = strategy->order_event_count.load() + 1; // We expect one more event
    while (strategy->order_event_count.load() < expected_count && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
        if (attempts % 10 == 0) {
            std::cout << "[TEST] Still waiting... order_event_count: " << strategy->order_event_count.load() 
                      << " last_event.cl_ord_id: " << strategy->last_event.cl_ord_id() << std::endl;
        }
    }
    std::cout << "[TEST] Final order_event_count: " << strategy->order_event_count.load() 
              << " last_event.cl_ord_id: " << strategy->last_event.cl_ord_id() << std::endl;

    // 7) Assertions
    CHECK(strategy->order_event_count.load() > 0);
    CHECK(strategy->last_event.cl_ord_id() == "TEST_ORDER_1");
    CHECK(strategy->last_event.symbol() == "BTCUSDT");
    CHECK(strategy->last_event.exch() == "binance");

    // Cleanup - stop trader lib first, then engine
    trader_lib->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    mock_ws->stop_event_loop();
    engine->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

