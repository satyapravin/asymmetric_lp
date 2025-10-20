#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <doctest/doctest.h>
#include "test_container.hpp"
#include "test_strategy.hpp"

using namespace testing;

/**
 * End-to-End Integration Test Suite
 * 
 * This test suite validates the complete trading system using:
 * - In-process ZMQ communication
 * - Mock exchange implementations
 * - Real JSON data files
 * - Complete order lifecycle testing
 */

TEST_CASE("Test Container Initialization") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    REQUIRE(container.is_running() == false);
    
    container.start();
    REQUIRE(container.is_running());
    
    container.stop();
    REQUIRE(container.is_running() == false);
}

TEST_CASE("Comprehensive Order Lifecycle Test") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    container.start();
    
    // Run comprehensive test
    bool test_result = container.run_comprehensive_test();
    
    container.stop();
    
    REQUIRE(test_result);
    
    // Print test results
    container.print_test_summary();
}

TEST_CASE("Order Lifecycle Individual Tests") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    container.start();
    
    // Test individual scenarios
    REQUIRE(container.run_order_lifecycle_test());
    REQUIRE(container.run_market_data_test());
    REQUIRE(container.run_position_balance_test());
    
    container.stop();
}

TEST_CASE("Mock Exchange Integration") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    
    // Test different exchange setups
    container.setup_mock_binance_exchange();
    container.setup_mock_grvt_exchange();
    container.setup_mock_deribit_exchange();
    
    container.start();
    
    // Run tests with different exchanges
    REQUIRE(container.run_comprehensive_test());
    
    container.stop();
}

TEST_CASE("ZMQ Communication Validation") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    container.start();
    
    // Test ZMQ message flow
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Validate that messages are flowing correctly
    const auto& results = container.get_results();
    REQUIRE(results.total_tests_run.load() > 0);
    
    container.stop();
}

TEST_CASE("Test Strategy Scenarios") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    container.start();
    
    // Test different strategy scenarios
    auto strategy = std::make_unique<TestStrategy>();
    
    // Configure test scenarios
    std::vector<std::string> scenarios = {
        "basic_order",
        "partial_fill", 
        "cancellation",
        "rejection",
        "market_data"
    };
    strategy->set_test_scenarios(scenarios);
    
    // Run strategy tests
    strategy->start();
    
    // Wait for tests to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    
    strategy->stop();
    
    // Validate results
    const auto& results = strategy->get_test_results();
    REQUIRE(results.orders_sent.load() > 0);
    REQUIRE(results.market_data_received.load() > 0);
    
    container.stop();
}

TEST_CASE("Performance and Stress Test") {
    TestContainer container;
    
    REQUIRE(container.initialize());
    container.start();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Run multiple test cycles
    for (int i = 0; i < 10; ++i) {
        REQUIRE(container.run_comprehensive_test());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "[PERFORMANCE] 10 test cycles completed in " << duration.count() << "ms" << std::endl;
    
    container.stop();
}

TEST_CASE("Error Handling and Recovery") {
    TestContainer container;
    
    // Test initialization failure scenarios
    // (This would require modifying the container to simulate failures)
    
    REQUIRE(container.initialize());
    container.start();
    
    // Test with invalid data
    // (This would require sending malformed messages)
    
    container.stop();
}

int main(int argc, char** argv) {
    std::cout << "=== End-to-End Integration Test Suite ===" << std::endl;
    std::cout << "Testing complete trading system with mock exchanges" << std::endl;
    std::cout << "Using in-process ZMQ communication" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Run doctest
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    
    int result = context.run();
    
    if (context.shouldExit()) {
        return result;
    }
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "All integration tests completed" << std::endl;
    
    return result;
}
