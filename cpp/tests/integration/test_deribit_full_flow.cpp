// Full flow integration test: Mock WS (Deribit market data) -> MarketServerLib -> ZMQ -> TraderLib ZmqMDSAdapter -> Strategy
#include "doctest.h"
#include "../mocks/mock_websocket_transport.hpp"
#include "../../market_server/market_server_lib.hpp"
#include "../../trader/trader_lib.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"
#include "../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../proto/market_data.pb.h"
#include "../../proto/acc_balance.pb.h"
#include "../../proto/position.pb.h"
#include "../../proto/order.pb.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>

class DeribitMarketDataCaptureStrategy : public AbstractStrategy {
public:
    std::atomic<int> orderbook_count{0};
    std::atomic<int> trade_count{0};
    proto::OrderBookSnapshot last_orderbook;
    proto::Trade last_trade;

    DeribitMarketDataCaptureStrategy() : AbstractStrategy("DeribitMarketDataCaptureStrategy") {}

    void start() override { running_.store(true); }
    void stop() override { running_.store(false); }

    void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
        orderbook_count++;
        last_orderbook = orderbook;
        std::cout << "[DERIBIT_STRATEGY] Received orderbook: " << orderbook.symbol() 
                  << " bids: " << orderbook.bids_size() << " asks: " << orderbook.asks_size() << std::endl;
    }

    void on_trade_execution(const proto::Trade& trade) override {
        trade_count++;
        last_trade = trade;
        std::cout << "[DERIBIT_STRATEGY] Received trade: " << trade.symbol() 
                  << " @ " << trade.price() << " qty: " << trade.qty() << std::endl;
    }

    // Unused in this test
    void on_order_event(const proto::OrderEvent&) override {}
    void on_position_update(const proto::PositionUpdate&) override {}
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

TEST_CASE("DERIBIT FULL FLOW INTEGRATION TEST - Market Server Direct") {
    std::cout << "\n=== DERIBIT FULL FLOW INTEGRATION TEST (Market Server Direct) ===" << std::endl;
    std::cout << "Flow: Mock WebSocket → Market Server → ZMQ → TraderLib → Strategy" << std::endl;

    // Use unique config with ports 6000-6004 to avoid conflicts with other tests
    const std::string config_file = "../../tests/config/test_config_deribit.ini";

    // Step 1: Create mock WebSocket transport and queue test messages
    std::cout << "\n[STEP 1] Creating mock WebSocket transport..." << std::endl;
    auto mock_transport = std::make_unique<test_utils::MockWebSocketTransport>();
    mock_transport->set_test_data_directory("data/deribit/websocket");
    REQUIRE(mock_transport != nullptr);
    
    // Queue the orderbook message BEFORE moving the transport
    mock_transport->load_and_replay_json_file("../tests/data/deribit/websocket/orderbook_message.json");
    mock_transport->load_and_replay_json_file("../tests/data/deribit/websocket/trade_message.json");

    // Step 2: Create MarketServerLib and inject mock WebSocket
    std::cout << "\n[STEP 2] Creating MarketServerLib..." << std::endl;
    auto market_server = std::make_unique<market_server::MarketServerLib>();
    
    // IMPORTANT: Set exchange and symbol BEFORE initializing (required)
    market_server->set_exchange("deribit");
    market_server->set_symbol("BTC-PERPETUAL");
    
    market_server->initialize(config_file);
    
    // Create ZmqPublisher for market server to publish to
    auto market_pub = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:6000");
    market_server->set_zmq_publisher(market_pub);
    
    // Inject the mock WebSocket transport
    market_server->set_websocket_transport(std::move(mock_transport));

    // Step 3: Create TraderLib with strategy
    std::cout << "\n[STEP 3] Creating TraderLib with strategy..." << std::endl;
    auto trader_lib = std::make_unique<trader::TraderLib>();
    trader_lib->set_exchange("deribit");
    trader_lib->set_symbol("BTC-PERPETUAL");
    trader_lib->initialize(config_file);
    
    auto strategy = std::make_shared<DeribitMarketDataCaptureStrategy>();
    trader_lib->set_strategy(strategy);
    
    // Give trader library time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    trader_lib->start();
    
    // Send mock balance and position updates to allow strategy to start
    // Strategy container requires these before it will forward market data
    proto::AccountBalanceUpdate balance_update;
    auto* balance = balance_update.add_balances();
    balance->set_exch("deribit");
    balance->set_instrument("USDC");
    balance->set_available(10000.0);
    balance->set_locked(0.0);
    trader_lib->simulate_balance_update(balance_update);
    
    proto::PositionUpdate position_update;
    position_update.set_exch("deribit");
    position_update.set_symbol("BTC-PERPETUAL");
    position_update.set_qty(0.0);
    trader_lib->simulate_position_update(position_update);
    
    // Send a mock order event to mark order state as queried (required for strategy to start)
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id("test_order");
    order_event.set_exch("deribit");
    order_event.set_symbol("BTC-PERPETUAL");
    order_event.set_event_type(proto::ACK);
    trader_lib->simulate_order_event(order_event);
    
    // Give strategy container time to process updates and start strategy
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Give ZMQ subscriber time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Step 4: Start market server (this will connect and subscribe, triggering message replay)
    std::cout << "\n[STEP 4] Starting market server..." << std::endl;
    market_server->start();
    
    // Give ZMQ pub-sub connection time to fully establish (ZMQ "slow joiner" problem)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 5: Wait for strategy to receive data
    // Note: Strategy container requires balance, position, and order state before starting
    // The timeout is 10 seconds for balance/position and 5 seconds for order state
    // So we need to wait at least 11 seconds for the strategy to fully start
    std::cout << "\n[STEP 5] Waiting for strategy to receive market data..." << std::endl;
    std::cout << "[STEP 5] Note: Strategy requires balance/position/order state before starting (timeout: 10s)" << std::endl;
    
    bool strategy_received_data = false;
    int wait_attempt = 0;
    const int max_wait_attempts = 120; // 12 seconds total (enough for 10s timeout + processing)
    
    while (wait_attempt < max_wait_attempts && !strategy_received_data) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_attempt++;
        auto strategy_ptr = trader_lib->get_strategy();
        if (strategy_ptr) {
            auto test_strategy_ptr = std::dynamic_pointer_cast<DeribitMarketDataCaptureStrategy>(strategy_ptr);
            if (test_strategy_ptr && test_strategy_ptr->orderbook_count.load() > 0) {
                strategy_received_data = true;
            }
        }
    }
    
    CHECK(strategy_received_data);
    
    if (strategy_received_data) {
        std::cout << "[TEST] ✅ Strategy received orderbook data" << std::endl;
        CHECK(strategy->last_orderbook.exch() == "DERIBIT");
        CHECK(strategy->last_orderbook.symbol() == "BTC-PERPETUAL");
        CHECK(strategy->last_orderbook.bids_size() > 0);
        CHECK(strategy->last_orderbook.asks_size() > 0);
    } else {
        std::cout << "[TEST] ⚠️ Strategy did not receive data within timeout" << std::endl;
    }

    // Cleanup - order matters for ZMQ: stop subscribers before publishers, stop threads before destroying contexts
    std::cout << "\n[CLEANUP] Stopping components..." << std::endl;
    
    // 1. Stop trader library first (stops ZMQ subscriber threads)
    trader_lib->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. Stop market server (stops ZMQ publisher)
    market_server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 3. Explicitly reset in reverse order to ensure clean destruction
    trader_lib.reset();
    market_server.reset();
    
    // 4. Final sleep to let ZMQ contexts terminate cleanly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "\n=== DERIBIT FULL FLOW INTEGRATION TEST COMPLETED ===" << std::endl;
}

