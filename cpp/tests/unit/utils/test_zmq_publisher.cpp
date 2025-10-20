#include "doctest.h"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include <thread>
#include <chrono>

TEST_CASE("ZmqPublisher - Basic Functionality") {
    // Test publisher creation and binding
    ZmqPublisher publisher("tcp://127.0.0.1:5559");
    
    // Give it a moment to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test sending a message
    std::string test_message = "Hello, ZMQ!";
    bool result = publisher.send_string("test_topic", test_message);
    CHECK(result == true);
}

TEST_CASE("ZmqPublisher - Invalid Endpoint") {
    // Test with invalid endpoint
    ZmqPublisher publisher("invalid://endpoint");
    
    // Should fail to bind
    std::string test_message = "This should fail";
    bool result = publisher.send_string("test_topic", test_message);
    CHECK(result == false);
}

TEST_CASE("ZmqPublisher - Empty Message") {
    ZmqPublisher publisher("tcp://127.0.0.1:5560");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test sending empty message
    bool result = publisher.send_string("test_topic", "");
    CHECK(result == true);
}

TEST_CASE("ZmqPublisher - Large Message") {
    ZmqPublisher publisher("tcp://127.0.0.1:5561");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test sending large message
    std::string large_message(10000, 'A');
    bool result = publisher.send_string("test_topic", large_message);
    CHECK(result == true);
}
