#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <chrono>
#include <thread>

// Include all test files

// Integration tests
#include "integration/test_full_chain_integration.cpp"
#include "integration/test_position_flow_integration.cpp"
#include "integration/test_order_flow_integration.cpp"

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