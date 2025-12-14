#include "doctest.h"
#include "../mocks/mock_websocket_transport.hpp"
#include "../../market_server/market_server_lib.hpp"
#include "../../trader/trader_lib.hpp"
#include "../../trader/zmq_mds_adapter.hpp"
#include "../../strategies/base_strategy/abstract_strategy.hpp"
#include "../../proto/market_data.pb.h"
#include "../../proto/order.pb.h"
#include "../../proto/position.pb.h"
#include "../../proto/acc_balance.pb.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

namespace integration_test {

// Test strategy that tracks received messages
class TestStrategy : public AbstractStrategy {
public:
    std::atomic<int> market_data_count{0};
    std::atomic<int> order_updates_count{0};
    std::atomic<int> position_updates_count{0};
    std::atomic<int> balance_updates_count{0};
    std::atomic<bool> connected_{false};
    
    // Store last received orderbook for verification
    proto::OrderBookSnapshot last_received_orderbook_;

    TestStrategy() : AbstractStrategy("TestStrategy") {}

    void start() override {
        std::cout << "[TEST_STRATEGY] Starting..." << std::endl;
        running_.store(true);
    }

    void stop() override {
        std::cout << "[TEST_STRATEGY] Stopping..." << std::endl;
        running_.store(false);
    }

    void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
        market_data_count++;
        std::cout << "[TEST_STRATEGY] ✅ RECEIVED MARKET DATA: " << orderbook.symbol()
                  << " bids: " << orderbook.bids_size() << " asks: " << orderbook.asks_size() 
                  << " (count: " << market_data_count.load() << ")" << std::endl;
        
        // Store the received data for verification
        last_received_orderbook_ = orderbook;
    }

    void on_trade_execution(const proto::Trade& trade) override {
        std::cout << "[TEST_STRATEGY] Trade execution: " << trade.symbol() << " @ " << trade.price() << std::endl;
    }

    void on_order_event(const proto::OrderEvent& order_event) override {
        order_updates_count++;
        std::cout << "[TEST_STRATEGY] Order event: " << order_event.cl_ord_id() << std::endl;
    }

    void on_position_update(const proto::PositionUpdate& position_update) override {
        position_updates_count++;
        std::cout << "[TEST_STRATEGY] Position update: " << position_update.symbol() << std::endl;
    }

    void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) override {
        balance_updates_count++;
        if (balance_update.balances_size() > 0) {
            std::cout << "[TEST_STRATEGY] Balance update: " << balance_update.balances(0).instrument() << std::endl;
        }
    }

    void on_connection_status(bool connected) {
        connected_ = connected;
        std::cout << "[TEST_STRATEGY] Connection status: " << (connected ? "CONNECTED" : "DISCONNECTED") << std::endl;
    }

    // Query methods (return empty for test)
    std::optional<trader::PositionInfo> get_position(const std::string& exchange, const std::string& symbol) const override {
        return std::nullopt;
    }
    std::vector<trader::PositionInfo> get_all_positions() const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string& exchange) const override { return {}; }
    std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string& symbol) const override { return {}; }

    std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string& exchange, const std::string& asset) const override {
        return std::nullopt;
    }
    std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const override { return {}; }
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const override { return {}; }

    // Order methods (return dummy for test)
    std::string send_order(const proto::OrderRequest& order_request) {
        std::cout << "[TEST_STRATEGY] Sending order: " << order_request.cl_ord_id() << std::endl;
        return order_request.cl_ord_id();
    }
    bool cancel_order(const std::string& client_order_id) {
        std::cout << "[TEST_STRATEGY] Cancelling order: " << client_order_id << std::endl;
        return true;
    }
    std::optional<proto::OrderEvent> get_order_status(const std::string& client_order_id) const {
        return std::nullopt;
    }
    std::vector<proto::OrderEvent> get_open_orders() const {
        return {};
    }
};

} // namespace integration_test

TEST_CASE("Full Chain Integration Test: Mock WebSocket → Market Server → Strategy") {
    std::cout << "\n=== FULL CHAIN INTEGRATION TEST ===" << std::endl;
    std::cout << "Flow: Mock WebSocket → Market Server → 0MQ → MDS Adapter → Strategy Container → Test Strategy" << std::endl;

    // Use unique config with ports 6100-6104 to avoid conflicts with other tests
    const std::string config_file = "../../tests/config/test_config_fullchain.ini";

    // Step 1: Create mock WebSocket transport
    std::cout << "\n[STEP 1] Creating mock WebSocket transport..." << std::endl;
    auto mock_transport = std::make_unique<test_utils::MockWebSocketTransport>();
    mock_transport->set_test_data_directory("data/binance/websocket");
    REQUIRE(mock_transport != nullptr);
    
    // Store reference to mock transport before moving it
    auto* mock_ws = mock_transport.get();

    // Step 2: Create Market Server
    std::cout << "\n[STEP 2] Creating Market Server..." << std::endl;
    auto market_server = std::make_unique<market_server::MarketServerLib>();
    REQUIRE(market_server != nullptr);

    // Step 3: Create test strategy
    std::cout << "\n[STEP 3] Creating test strategy..." << std::endl;
    auto test_strategy = std::make_unique<integration_test::TestStrategy>();
    REQUIRE(test_strategy != nullptr);

    // Step 4: Create trader library (like in production)
    std::cout << "\n[STEP 4] Creating trader library..." << std::endl;
    auto trader_lib = std::make_unique<trader::TraderLib>();
    REQUIRE(trader_lib != nullptr);
    
    // Configure trader library
    trader_lib->set_symbol("BTCUSDT");
    trader_lib->set_exchange("binance");
    trader_lib->initialize(config_file);
    
    // TraderLib::initialize() configures all adapters internally
    
    // Step 5: Set up the chain (start publisher before trader subscribes)
    std::cout << "\n[STEP 5] Setting up the data flow chain..." << std::endl;
    // IMPORTANT: Set exchange and symbol BEFORE initializing (required)
    market_server->set_exchange("binance");
    market_server->set_symbol("BTCUSDT");
    // Initialize Market Server (this will set up 0MQ publishing)
    market_server->initialize(config_file);
    
    // Create ZmqPublisher for market server to publish to
    auto market_pub = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:6100");
    market_server->set_zmq_publisher(market_pub);
    
    // Inject the mock WebSocket transport into Market Server
    market_server->set_websocket_transport(std::move(mock_transport));
    market_server->start();
    // Give publisher time to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Set the strategy in trader (this will set up the MDS adapter callback)
    trader_lib->set_strategy(std::shared_ptr<integration_test::TestStrategy>(test_strategy.release()));
    // Start trader library after publisher is up
    trader_lib->start();
    
    // Send mock balance and position updates to allow strategy to start
    // Strategy container requires these before it will forward market data
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
    
    // Send a mock order event to mark order state as queried (required for strategy to start)
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id("test_order");
    order_event.set_exch("binance");
    order_event.set_symbol("BTCUSDT");
    order_event.set_event_type(proto::ACK);
    trader_lib->simulate_order_event(order_event);
    
    // Give strategy container time to process updates and start strategy
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 6: Connect and simulate data flow
    std::cout << "\n[STEP 6] Connecting and simulating data flow..." << std::endl;
    
    // Start the simulation loop to process queued messages
    mock_ws->start_event_loop();
    mock_ws->simulate_connection_success();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Use the mock WebSocket to replay snapshot message (we use snapshots, not streams)
    REQUIRE(mock_ws != nullptr);
    mock_ws->load_and_replay_json_file("../tests/data/binance/websocket/orderbook_snapshot_message.json");
    
    std::cout << "[SIMULATION] Mock WebSocket sent orderbook message" << std::endl;
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 7: Verify the full chain worked
    std::cout << "\n[STEP 7] Verifying full chain..." << std::endl;
    
    // Wait for the strategy to receive the market data
    // Note: Strategy container requires balance, position, and order state before starting
    // The timeout is 10 seconds for balance/position and 5 seconds for order state
    // So we need to wait at least 11 seconds for the strategy to fully start
    std::cout << "[VERIFICATION] Waiting for strategy to receive market data..." << std::endl;
    std::cout << "[VERIFICATION] Note: Strategy requires balance/position/order state before starting (timeout: 10s)" << std::endl;
    int max_wait_attempts = 120; // 12 seconds total (enough for 10s timeout + processing)
    int wait_attempt = 0;
    bool strategy_received_data = false;
    
    while (wait_attempt < max_wait_attempts && !strategy_received_data) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_attempt++;
        
        // Get the strategy from the trader library to check counters
        auto strategy = trader_lib->get_strategy();
        if (strategy) {
            // Cast to TestStrategy to access counters
            auto test_strategy_ptr = std::dynamic_pointer_cast<integration_test::TestStrategy>(strategy);
            if (test_strategy_ptr && test_strategy_ptr->market_data_count.load() > 0) {
                strategy_received_data = true;
                std::cout << "[VERIFICATION] ✅ Strategy received market data after " << (wait_attempt * 100) << "ms" << std::endl;
                std::cout << "[VERIFICATION] Market data count: " << test_strategy_ptr->market_data_count.load() << std::endl;
            }
        }
        
        if (wait_attempt % 10 == 0) { // Log every second
            std::cout << "[VERIFICATION] Still waiting... (" << wait_attempt * 100 << "ms)" << std::endl;
        }
    }
    
    // Assertions to verify the full chain worked
    std::cout << "[VERIFICATION] Trader library is running: " << trader_lib->is_running() << std::endl;
    std::cout << "[VERIFICATION] Strategy received data: " << strategy_received_data << std::endl;
    
    // Critical assertions
    CHECK(trader_lib->is_running() == true);
    CHECK(strategy_received_data == true);
    
    if (!strategy_received_data) {
        std::cerr << "[ERROR] Strategy did not receive market data within 5 seconds!" << std::endl;
    }
    
    // Verify the parsed data matches the JSON file exactly
    std::cout << "\n[DATA_VERIFICATION] Verifying parsed data matches JSON file..." << std::endl;
    auto strategy = trader_lib->get_strategy();
    auto test_strategy_ptr = std::dynamic_pointer_cast<integration_test::TestStrategy>(strategy);
    
    if (test_strategy_ptr && test_strategy_ptr->market_data_count.load() > 0) {
        const auto& orderbook = test_strategy_ptr->last_received_orderbook_;
        
        // Verify symbol
        std::cout << "[DATA_VERIFICATION] Symbol: " << orderbook.symbol() << " (expected: BTCUSDT)" << std::endl;
        CHECK(orderbook.symbol() == "BTCUSDT");
        
        // Verify exchange
        std::cout << "[DATA_VERIFICATION] Exchange: " << orderbook.exch() << " (expected: binance)" << std::endl;
        CHECK(orderbook.exch() == "binance");
        
        // Verify timestamp
        std::cout << "[DATA_VERIFICATION] Timestamp: " << orderbook.timestamp_us() << " (expected: 1640995200000)" << std::endl;
        CHECK(orderbook.timestamp_us() == 1640995200000);
        
        // Verify bids count (depth20 snapshot uses 4 levels in fixture)
        std::cout << "[DATA_VERIFICATION] Bids count: " << orderbook.bids_size() << " (expected: 4)" << std::endl;
        CHECK(orderbook.bids_size() == 4);
        
        // Verify asks count (depth20 snapshot uses 4 levels in fixture)
        std::cout << "[DATA_VERIFICATION] Asks count: " << orderbook.asks_size() << " (expected: 4)" << std::endl;
        CHECK(orderbook.asks_size() == 4);
        
        // Verify bid prices and quantities (from JSON: [49999.0, 0.1], [49998.0, 0.2])
        if (orderbook.bids_size() >= 2) {
            std::cout << "[DATA_VERIFICATION] Bid 0: price=" << orderbook.bids(0).price() 
                      << " qty=" << orderbook.bids(0).qty() << " (expected: 49999.0, 0.1)" << std::endl;
            CHECK(std::abs(orderbook.bids(0).price() - 49999.0) < 0.0001);
            CHECK(std::abs(orderbook.bids(0).qty() - 0.1) < 0.0001);
            
            std::cout << "[DATA_VERIFICATION] Bid 1: price=" << orderbook.bids(1).price() 
                      << " qty=" << orderbook.bids(1).qty() << " (expected: 49998.0, 0.2)" << std::endl;
            CHECK(std::abs(orderbook.bids(1).price() - 49998.0) < 0.0001);
            CHECK(std::abs(orderbook.bids(1).qty() - 0.2) < 0.0001);
        }
        
        // Verify ask prices and quantities (from JSON: [50001.0, 0.15], [50002.0, 0.25])
        if (orderbook.asks_size() >= 2) {
            std::cout << "[DATA_VERIFICATION] Ask 0: price=" << orderbook.asks(0).price() 
                      << " qty=" << orderbook.asks(0).qty() << " (expected: 50001.0, 0.15)" << std::endl;
            CHECK(std::abs(orderbook.asks(0).price() - 50001.0) < 0.0001);
            CHECK(std::abs(orderbook.asks(0).qty() - 0.15) < 0.0001);
            
            std::cout << "[DATA_VERIFICATION] Ask 1: price=" << orderbook.asks(1).price() 
                      << " qty=" << orderbook.asks(1).qty() << " (expected: 50002.0, 0.25)" << std::endl;
            CHECK(std::abs(orderbook.asks(1).price() - 50002.0) < 0.0001);
            CHECK(std::abs(orderbook.asks(1).qty() - 0.25) < 0.0001);
        }
        
        std::cout << "[DATA_VERIFICATION] ✅ All data verification assertions passed!" << std::endl;
    } else {
        std::cerr << "[ERROR] Could not access strategy data for verification!" << std::endl;
        CHECK(false); // Fail the test
    }
    
    // Step 8: Cleanup
    std::cout << "\n[STEP 8] Cleaning up..." << std::endl;
    
    // Stop Trader Library first (stops receiving messages)
    trader_lib->stop();
    
    // Give threads time to clean up
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Stop Market Server (stops publishing messages)
    market_server->stop();
    
    // Stop mock WebSocket event loop (if it's still running)
    if (mock_ws && mock_ws->is_event_loop_running()) {
        mock_ws->stop_event_loop();
    }
    
    // Give threads and ZMQ resources time to fully clean up
    // ZMQ needs extra time to clean up sockets and contexts
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "\n=== FULL CHAIN INTEGRATION TEST COMPLETED ===" << std::endl;
}