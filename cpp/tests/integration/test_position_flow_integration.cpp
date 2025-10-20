#include "doctest.h"
#include "../trader/trader_lib.hpp"
#include "../position_server/position_server_lib.hpp"
#include "../tests/mocks/mock_websocket_transport.hpp"
#include "../exchanges/binance/private_websocket/binance_pms.hpp"
#include <iostream>
#include <thread>
#include <chrono>

TEST_CASE("POSITION FLOW INTEGRATION TEST") {
    std::cout << "\n=== POSITION FLOW INTEGRATION TEST ===" << std::endl;
    std::cout << "Flow: Mock WebSocket → Binance PMS → Position Server → 0MQ → PMS Adapter → TraderLib → Strategy Container → Strategy" << std::endl;
    std::cout << std::endl;

    // Test strategy to receive position updates
    class TestPositionStrategy : public AbstractStrategy {
    public:
        TestPositionStrategy() : AbstractStrategy("TestPositionStrategy"), position_count_(0) {}
        
        void start() override {
            std::cout << "[TEST_POSITION_STRATEGY] Starting test strategy" << std::endl;
            running_.store(true);
        }
        
        void stop() override {
            std::cout << "[TEST_POSITION_STRATEGY] Stopping test strategy" << std::endl;
            running_.store(false);
        }
        
        void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
            // Not used in this test
        }
        
        void on_order_event(const proto::OrderEvent& order_event) override {
            // Not used in this test
        }
        
        void on_position_update(const proto::PositionUpdate& position) override {
            position_count_++;
            last_position_ = position;
            
            std::cout << "[TEST_POSITION_STRATEGY] ✅ RECEIVED POSITION UPDATE: " 
                      << position.symbol() << " qty: " << position.qty() 
                      << " price: " << position.avg_price() << " (count: " << position_count_ << ")" << std::endl;
        }
        
        void on_trade_execution(const proto::Trade& trade) override {
            // Not used in this test
        }
        
        // Implement required abstract methods
        void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) override {
            // Not used in this test
        }
        
        std::optional<trader::PositionInfo> get_position(const std::string& exchange, const std::string& symbol) const override {
            return std::nullopt;
        }
        
        std::vector<trader::PositionInfo> get_all_positions() const override {
            return {};
        }
        
        std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string& exchange) const override {
            return {};
        }
        
        std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string& symbol) const override {
            return {};
        }
        
        std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string& exchange, const std::string& instrument) const override {
            return std::nullopt;
        }
        
        std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override {
            return {};
        }
        
        std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const override {
            return {};
        }
        
        std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const override {
            return {};
        }
        
        int get_position_count() const { return position_count_; }
        const proto::PositionUpdate& get_last_position() const { return last_position_; }
        
    private:
        std::atomic<int> position_count_;
        proto::PositionUpdate last_position_;
    };

    std::cout << "[STEP 1] Creating mock WebSocket transport..." << std::endl;
    auto mock_transport = std::make_unique<test_utils::MockWebSocketTransport>();
    mock_transport->set_test_data_directory("data/binance/websocket");
    
    std::cout << "[STEP 2] Creating test strategy..." << std::endl;
    auto test_strategy = std::make_shared<TestPositionStrategy>();
    
    std::cout << "[STEP 3] Creating trader library..." << std::endl;
    auto trader_lib = std::make_unique<trader::TraderLib>();
    
    std::cout << "[STEP 4] Creating position server..." << std::endl;
    auto position_server = std::make_unique<position_server::PositionServerLib>();
    
    std::cout << "[STEP 5] Setting up trader library..." << std::endl;
    trader_lib->initialize("test_config.ini");
    trader_lib->set_strategy(test_strategy);
    
    trader_lib->start();
    
    std::cout << "[STEP 6] Setting up position server..." << std::endl;
    position_server->initialize("test_config.ini");
    
    // Create ZMQ publisher for position server
    auto zmq_publisher = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5556");
    position_server->set_zmq_publisher(zmq_publisher);
    
    std::cout << "[STEP 7] Injecting mock WebSocket transport..." << std::endl;
    auto mock_transport_shared = std::shared_ptr<test_utils::MockWebSocketTransport>(mock_transport.release());
    position_server->set_websocket_transport(mock_transport_shared);
    
    std::cout << "[STEP 8] Starting position server..." << std::endl;
    position_server->start();
    
    std::cout << "[STEP 9] Waiting for PMS adapter to establish ZMQ connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "[STEP 10] Starting mock WebSocket event loop..." << std::endl;
    mock_transport_shared->start_event_loop();
    
    std::cout << "[STEP 11] Sending position update message..." << std::endl;
    // Send position update message from mock WebSocket
    mock_transport_shared->simulate_custom_message(R"({"e":"ACCOUNT_UPDATE","E":1640995200000,"T":1640995200000,"a":{"B":[{"a":"USDT","wb":"10000.00000000","cw":"10000.00000000"}],"P":[{"s":"BTCUSDT","pa":"0.1","ep":"50000.00","cr":"0.00","up":"10.00","mt":"isolated","iw":"0.00","ps":"LONG"}],"m":"UPDATE"}})");
    
    std::cout << "[STEP 12] Waiting for position update to propagate..." << std::endl;
    
    // Wait for the position update to propagate through the chain
    int attempts = 0;
    while (test_strategy->get_position_count() == 0 && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    std::cout << "[STEP 13] Verifying position flow..." << std::endl;
    
    // Verify the strategy received the position update
    CHECK(test_strategy->get_position_count() > 0);
    CHECK(test_strategy->get_position_count() == 1);
    
    const auto& received_position = test_strategy->get_last_position();
    CHECK(received_position.symbol() == "BTCUSDT");
    CHECK(received_position.qty() == 0.1);
    CHECK(received_position.avg_price() == 50000.0);
    CHECK(received_position.exch() == "binance");
    
    std::cout << "[VERIFICATION] ✅ Position update received successfully!" << std::endl;
    std::cout << "[VERIFICATION] Symbol: " << received_position.symbol() << std::endl;
    std::cout << "[VERIFICATION] Quantity: " << received_position.qty() << std::endl;
    std::cout << "[VERIFICATION] Average Price: " << received_position.avg_price() << std::endl;
    std::cout << "[VERIFICATION] Exchange: " << received_position.exch() << std::endl;
    
    std::cout << "[STEP 14] Cleaning up..." << std::endl;
    
    // Clean up
    trader_lib->stop();
    position_server->stop();
    
    std::cout << "=== POSITION FLOW INTEGRATION TEST COMPLETED ===" << std::endl;
}
