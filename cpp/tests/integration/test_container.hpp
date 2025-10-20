#pragma once
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <zmq.hpp>
#include "../integration/test_strategy.hpp"
#include "../../trader/trader_lib.hpp"
#include "../../trading_engine/trading_engine_lib.hpp"
#include "../../market_server/market_server_lib.hpp"
#include "../../position_server/position_server_lib.hpp"
#include "../../exchanges/i_exchange_oms.hpp"
#include "../../exchanges/i_exchange_subscriber.hpp"
#include "../../exchanges/i_exchange_pms.hpp"
#include "../../exchanges/i_exchange_data_fetcher.hpp"
#include "../../proto/order.pb.h"
#include "../../proto/market_data.pb.h"
#include "../../proto/position.pb.h"
#include "../../proto/acc_balance.pb.h"

namespace testing {

/**
 * Test Container for End-to-End Testing
 * 
 * This container integrates all services using in-process ZMQ:
 * - Mock Exchange OMS (order management)
 * - Mock Exchange Subscriber (market data)
 * - Mock Exchange PMS (position/balance management)
 * - Mock Exchange Data Fetcher (HTTP endpoints)
 * - Strategy Container with Test Strategy
 * - ZMQ Publishers/Subscribers for inter-service communication
 */
class TestContainer {
public:
    TestContainer();
    ~TestContainer();

    // Container lifecycle
    bool initialize();
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Test execution
    bool run_comprehensive_test();
    bool run_order_lifecycle_test();
    bool run_market_data_test();
    bool run_position_balance_test();

    // Mock exchange setup
    void setup_mock_binance_exchange();
    void setup_mock_grvt_exchange();
    void setup_mock_deribit_exchange();

    // Test results
    struct TestContainerResults {
        std::atomic<bool> initialization_success{false};
        std::atomic<bool> order_test_success{false};
        std::atomic<bool> market_data_test_success{false};
        std::atomic<bool> position_balance_test_success{false};
        std::atomic<uint64_t> total_tests_run{0};
        std::atomic<uint64_t> total_tests_passed{0};
        std::atomic<uint64_t> total_tests_failed{0};
        
        std::chrono::system_clock::time_point test_start_time;
        std::chrono::system_clock::time_point test_end_time;
        
        void reset() {
            initialization_success.store(false);
            order_test_success.store(false);
            market_data_test_success.store(false);
            position_balance_test_success.store(false);
            total_tests_run.store(0);
            total_tests_passed.store(0);
            total_tests_failed.store(0);
        }
    };

    const TestContainerResults& get_results() const { return results_; }
    void print_test_summary() const;

private:
    std::atomic<bool> running_;
    TestContainerResults results_;
    
    // Core components
    std::unique_ptr<TestStrategy> test_strategy_;
    std::unique_ptr<StrategyContainer> strategy_container_;
    std::unique_ptr<MiniOMS> mini_oms_;
    std::unique_ptr<MiniPMS> mini_pms_;
    
    // Mock exchange implementations
    std::unique_ptr<IExchangeOMS> mock_oms_;
    std::unique_ptr<IExchangeSubscriber> mock_subscriber_;
    std::unique_ptr<IExchangePMS> mock_pms_;
    std::unique_ptr<IExchangeDataFetcher> mock_data_fetcher_;
    
    // ZMQ components for in-process communication
    std::unique_ptr<zmq::context_t> zmq_context_;
    std::unique_ptr<zmq::socket_t> oms_publisher_;
    std::unique_ptr<zmq::socket_t> oms_subscriber_;
    std::unique_ptr<zmq::socket_t> mds_subscriber_;
    std::unique_ptr<zmq::socket_t> pms_subscriber_;
    
    // Test configuration
    std::string test_symbol_{"BTCUSDT"};
    std::string test_exchange_{"BINANCE"};
    std::string test_data_path_{"../../tests/data/binance/"};
    
    // Internal methods
    bool initialize_zmq_components();
    bool initialize_mock_exchanges();
    bool initialize_strategy_container();
    bool setup_zmq_bindings();
    
    // Test execution helpers
    bool wait_for_condition(std::function<bool()> condition, 
                           std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    void simulate_market_data();
    void simulate_order_responses();
    void simulate_position_updates();
    void simulate_balance_updates();
    
    // Mock data loading
    std::string load_json_file(const std::string& file_path);
    bool parse_orderbook_snapshot(const std::string& json_data, proto::OrderBookSnapshot& snapshot);
    bool parse_order_update(const std::string& json_data, proto::OrderEvent& order_event);
    bool parse_position_update(const std::string& json_data, proto::PositionUpdate& position);
    bool parse_balance_update(const std::string& json_data, proto::AccountBalanceUpdate& balance);
    
    // Test validation
    bool validate_order_lifecycle();
    bool validate_market_data_flow();
    bool validate_position_balance_flow();
    bool validate_zmq_communication();
};

} // namespace testing
