#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <chrono>
#include <thread>

// Include all test files
#include "unit/position_server/test_position_server_factory.cpp"
#include "unit/utils/test_exchange_oms_factory.cpp"
#include "unit/utils/test_mock_exchange_oms.cpp"
#include "unit/utils/test_oms.cpp"
#include "unit/trader/test_market_making_strategy.cpp"
#include "unit/utils/test_zmq.cpp"

// Integration tests
#include "integration/test_integration.cpp"

// Add timeout to prevent hanging
int main(int argc, char** argv) {
    // Set a timeout for the entire test suite
    std::thread timeout_thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(120));
        std::cout << "\n[TEST_RUNNER] Timeout reached, forcing exit..." << std::endl;
        std::exit(1);
    });
    timeout_thread.detach();
    
    return doctest::Context(argc, argv).run();
}
