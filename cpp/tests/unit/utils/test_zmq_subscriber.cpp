#include "doctest.h"
#include "../../../utils/zmq/zmq_subscriber.hpp"
#include "../../../utils/zmq/zmq_publisher.hpp"
#include <thread>
#include <chrono>
#include <atomic>

TEST_CASE("ZmqSubscriber - Basic Functionality") {
    // Create publisher and subscriber
    ZmqPublisher publisher("tcp://127.0.0.1:5562");
    ZmqSubscriber subscriber("tcp://127.0.0.1:5562", "test_topic");
    
    // Give them time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send a message
    std::string test_message = "Hello, Subscriber!";
    bool sent = publisher.send_string("test_topic", test_message);
    CHECK(sent == true);
    
    // Receive the message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto received = subscriber.receive_blocking(1000);
    CHECK(received.has_value());
    CHECK(received.value() == test_message);
}

TEST_CASE("ZmqSubscriber - Topic Filtering") {
    ZmqPublisher publisher("tcp://127.0.0.1:5563");
    ZmqSubscriber subscriber("tcp://127.0.0.1:5563", "filtered_topic");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send message to different topic
    publisher.send_string("other_topic", "Should not receive this");
    
    // Send message to correct topic
    std::string correct_message = "This should be received";
    publisher.send_string("filtered_topic", correct_message);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should only receive the filtered message
    auto received = subscriber.receive_blocking(1000);
    CHECK(received.has_value());
    CHECK(received.value() == correct_message);
}

TEST_CASE("ZmqSubscriber - Timeout") {
    ZmqSubscriber subscriber("tcp://127.0.0.1:5564", "timeout_topic");
    
    // Try to receive with short timeout
    auto received = subscriber.receive_blocking(100);
    CHECK(!received.has_value());
}

TEST_CASE("ZmqSubscriber - Multiple Messages") {
    ZmqPublisher publisher("tcp://127.0.0.1:5565");
    ZmqSubscriber subscriber("tcp://127.0.0.1:5565", "multi_topic");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send multiple messages
    std::vector<std::string> messages = {"Message 1", "Message 2", "Message 3"};
    for (const auto& msg : messages) {
        publisher.send_string("multi_topic", msg);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Receive all messages
    std::vector<std::string> received_messages;
    for (int i = 0; i < 3; ++i) {
        auto received = subscriber.receive_blocking(1000);
        if (received.has_value()) {
            received_messages.push_back(received.value());
        }
    }
    
    CHECK(received_messages.size() == 3);
    CHECK(received_messages[0] == "Message 1");
    CHECK(received_messages[1] == "Message 2");
    CHECK(received_messages[2] == "Message 3");
}
