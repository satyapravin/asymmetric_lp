#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <chrono>
#include <thread>

// Include all test files

// Unit tests - Core utilities (working tests)
#include "unit/utils/test_zmq_publisher.cpp"
#include "unit/utils/test_zmq_subscriber.cpp"
#include "unit/config/test_process_config_manager.cpp"

// Unit tests - Exchange implementations
#include "unit/exchanges/test_grvt_oms.cpp"
#include "unit/exchanges/test_deribit_oms.cpp"

// Integration tests
#include "integration/test_full_chain_integration.cpp"
#include "integration/test_position_flow_integration.cpp"
#include "integration/test_order_flow_integration.cpp"
#include "integration/test_grvt_integration.cpp"
#include "integration/test_grvt_full_flow.cpp"
#include "integration/test_deribit_integration.cpp"
#include "integration/test_deribit_full_flow.cpp"

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