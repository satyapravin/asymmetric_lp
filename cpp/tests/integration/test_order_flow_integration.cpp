#include "doctest.h"
#include "../../trader/trader_lib.hpp"
#include "../../trading_engine/trading_engine_lib.hpp"
#include "../../tests/mocks/mock_websocket_transport.hpp"
#include "../../exchanges/binance/private_websocket/binance_oms.hpp"
#include <iostream>
#include <thread>
#include <chrono>

TEST_CASE("ORDER FLOW INTEGRATION TEST") {
    std::cout << "\n=== ORDER FLOW INTEGRATION TEST ===" << std::endl;
    std::cout << "Flow: Mock WebSocket → Binance OMS → Trading Engine → 0MQ → OMS Adapter → TraderLib → Strategy Container → Strategy" << std::endl;
    std::cout << std::endl;

    // Test strategy to receive order events
    class TestOrderStrategy : public AbstractStrategy {
    public:
        TestOrderStrategy() : AbstractStrategy("TestOrderStrategy"), order_count_(0) {}
        
        void start() override {
            std::cout << "[TEST_ORDER_STRATEGY] Starting test strategy" << std::endl;
            running_.store(true);
        }
        
        void stop() override {
            std::cout << "[TEST_ORDER_STRATEGY] Stopping test strategy" << std::endl;
            running_.store(false);
        }
        
        void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
            // Not used in this test
        }
        
        void on_order_event(const proto::OrderEvent& order_event) override {
            order_count_++;
            last_order_event_ = order_event;
            
            std::cout << "[TEST_ORDER_STRATEGY] ✅ RECEIVED ORDER EVENT: " 
                      << order_event.cl_ord_id() << " status: " << order_event.status() 
                      << " (count: " << order_count_ << ")" << std::endl;
        }
        
        void on_position_update(const proto::PositionUpdate& position) override {
            // Not used in this test
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
        
        int get_order_count() const { return order_count_; }
        const proto::OrderEvent& get_last_order_event() const { return last_order_event_; }
        
    private:
        std::atomic<int> order_count_;
        proto::OrderEvent last_order_event_;
    };

    std::cout << "[STEP 1] Creating mock WebSocket transport..." << std::endl;
    auto mock_transport = std::make_unique<test_utils::MockWebSocketTransport>();
    mock_transport->set_test_data_directory("data/binance/websocket");
    
    std::cout << "[STEP 2] Creating test strategy..." << std::endl;
    auto test_strategy = std::make_shared<TestOrderStrategy>();
    
    std::cout << "[STEP 3] Creating trader library..." << std::endl;
    auto trader_lib = std::make_unique<trader::TraderLib>();
    
    std::cout << "[STEP 4] Creating trading engine..." << std::endl;
    auto trading_engine = std::make_unique<trading_engine::TradingEngineLib>();
    
    std::cout << "[STEP 5] Setting up trader library..." << std::endl;
    trader_lib->initialize("test_config.ini");
    trader_lib->set_strategy(test_strategy);
    
    trader_lib->start();
    
    std::cout << "[STEP 6] Setting up trading engine..." << std::endl;
    trading_engine->initialize("test_config.ini");
    
    // Create ZMQ publisher for trading engine
    auto zmq_publisher = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5557");
    trading_engine->set_zmq_publisher(zmq_publisher);
    
    std::cout << "[STEP 7] Injecting mock WebSocket transport..." << std::endl;
    auto mock_transport_shared = std::shared_ptr<test_utils::MockWebSocketTransport>(mock_transport.release());
    trading_engine->set_websocket_transport(mock_transport_shared);
    
    std::cout << "[STEP 8] Starting trading engine..." << std::endl;
    trading_engine->start();
    
    std::cout << "[STEP 9] Waiting for OMS adapter to establish ZMQ connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "[STEP 10] Starting mock WebSocket event loop..." << std::endl;
    mock_transport_shared->start_event_loop();
    
    std::cout << "[STEP 11] Sending order response message..." << std::endl;
    // Send order response message from mock WebSocket
    mock_transport_shared->simulate_custom_message(R"({"e":"ORDER_TRADE_UPDATE","E":1640995200000,"T":1640995200000,"o":{"s":"BTCUSDT","c":"TEST_ORDER_123","S":"BUY","o":"LIMIT","q":"0.1","p":"50000.00","ap":"0.00000000","sp":"0.00000000","x":"NEW","X":"NEW","i":123456789,"l":"0.00000000","z":"0.00000000","L":"0.00000000","n":"0","N":null,"T":1640995200000,"t":0,"b":"0.00000000","a":"0.00000000","m":false,"R":false,"wt":"CONTRACT_PRICE","ot":"LIMIT","ps":"NONE","cp":false,"rp":"0.00000000","pP":false,"si":0,"ss":0,"tf":0}})");
    
    std::cout << "[STEP 12] Waiting for order event to propagate..." << std::endl;
    
    // Wait for the order event to propagate through the chain
    int attempts = 0;
    while (test_strategy->get_order_count() == 0 && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    std::cout << "[STEP 13] Verifying order flow..." << std::endl;
    
    // Verify the strategy received the order event
    CHECK(test_strategy->get_order_count() > 0);
    CHECK(test_strategy->get_order_count() == 1);
    
    const auto& received_order = test_strategy->get_last_order_event();
    CHECK(received_order.cl_ord_id() == "TEST_ORDER_123");
    CHECK(received_order.status() == proto::OrderStatus::NEW);
    CHECK(received_order.symbol() == "BTCUSDT");
    CHECK(received_order.side() == proto::Side::BUY);
    CHECK(received_order.qty() == doctest::Approx(0.1));
    CHECK(received_order.price() == doctest::Approx(50000.0));
    CHECK(received_order.exch() == "binance");
    
    std::cout << "[VERIFICATION] ✅ Order event received successfully!" << std::endl;
    std::cout << "[VERIFICATION] Client Order ID: " << received_order.cl_ord_id() << std::endl;
    std::cout << "[VERIFICATION] Status: " << received_order.status() << std::endl;
    std::cout << "[VERIFICATION] Symbol: " << received_order.symbol() << std::endl;
    std::cout << "[VERIFICATION] Side: " << received_order.side() << std::endl;
    std::cout << "[VERIFICATION] Quantity: " << received_order.qty() << std::endl;
    std::cout << "[VERIFICATION] Price: " << received_order.price() << std::endl;
    std::cout << "[VERIFICATION] Exchange: " << received_order.exch() << std::endl;
    
    std::cout << "[STEP 14] Cleaning up..." << std::endl;
    
    // Clean up
    trader_lib->stop();
    trading_engine->stop();
    
    std::cout << "=== ORDER FLOW INTEGRATION TEST COMPLETED ===" << std::endl;
}


