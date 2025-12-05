// Full flow integration test: Mock WS (Deribit market data) -> MarketServerLib -> ZMQ -> TraderLib ZmqMDSAdapter -> Strategy
#include "doctest.h"
#include "../mocks/mock_websocket_transport.hpp"
#include "../../market_server/market_server_lib.hpp"
#include "../../trader/trader_lib.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"
#include "../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../proto/market_data.pb.h"
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
    
    market_server->initialize("../tests/test_config.ini");
    
    // Inject the mock WebSocket transport
    market_server->set_websocket_transport(std::move(mock_transport));

    // Step 3: Create TraderLib with strategy
    std::cout << "\n[STEP 3] Creating TraderLib with strategy..." << std::endl;
    auto trader_lib = std::make_unique<trader::TraderLib>();
    trader_lib->set_exchange("deribit");
    trader_lib->set_symbol("BTC-PERPETUAL");
    trader_lib->initialize("../tests/test_config.ini");
    
    auto strategy = std::make_shared<DeribitMarketDataCaptureStrategy>();
    trader_lib->set_strategy(strategy);
    
    // Give trader library time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    trader_lib->start();
    
    // Give ZMQ subscriber time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Step 4: Start market server (this will connect and subscribe, triggering message replay)
    std::cout << "\n[STEP 4] Starting market server..." << std::endl;
    market_server->start();
    
    // Give ZMQ pub-sub connection time to fully establish (ZMQ "slow joiner" problem)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 5: Wait for strategy to receive data
    std::cout << "\n[STEP 5] Waiting for strategy to receive market data..." << std::endl;
    
    bool strategy_received_data = false;
    int wait_attempt = 0;
    const int max_wait_attempts = 50;
    
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

    // Cleanup - order matters: stop trader lib first (which owns the subscriber), then market server
    std::cout << "\n[CLEANUP] Stopping components..." << std::endl;
    
    // Stop trader library first (stops receiving messages)
    trader_lib->stop();
    
    // Give threads time to clean up
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Stop market server (stops publishing messages)
    market_server->stop();
    
    // Give threads and ZMQ resources time to fully clean up
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "\n=== DERIBIT FULL FLOW INTEGRATION TEST COMPLETED ===" << std::endl;
}

