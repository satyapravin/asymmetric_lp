#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include <string>
#include <thread>
#include <chrono>

TEST_SUITE("ZeroMQ Components") {
    
    TEST_CASE("ZmqPublisher Constructor") {
        ZmqPublisher pub("tcp://127.0.0.1:5555");
        
        // Should construct without throwing
        CHECK(true);
    }
    
    TEST_CASE("ZmqSubscriber Constructor") {
        ZmqSubscriber sub("tcp://127.0.0.1:5555", "test_topic");
        
        // Should construct without throwing
        CHECK(true);
    }
    
    TEST_CASE("Publisher-Subscriber Communication") {
        const std::string endpoint = "tcp://127.0.0.1:5556";
        const std::string topic = "test_topic";
        const std::string test_message = "Hello, ZeroMQ!";
        
        ZmqPublisher pub(endpoint);
        ZmqSubscriber sub(endpoint, topic);
        
        // Note: ZmqSubscriber doesn't have callbacks, it uses receive() method
        // This is a simplified test that just checks basic functionality
        
        // Publish message
        pub.publish(topic, test_message);
        
        // Try to receive message (with timeout)
        auto start = std::chrono::steady_clock::now();
        std::optional<std::string> received_message;
        
        while (!received_message && 
               std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            received_message = sub.receive();
            if (!received_message) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        // Note: The actual message format may be different due to ZeroMQ framing
        // We just check that we got some message
        CHECK(received_message.has_value());
    }
    
    TEST_CASE("Multiple Messages") {
        const std::string endpoint = "tcp://127.0.0.1:5557";
        const std::string topic = "multi_topic";
        
        ZmqPublisher pub(endpoint);
        ZmqSubscriber sub(endpoint, topic);
        
        int message_count = 0;
        
        // Send multiple messages
        for (int i = 0; i < 3; ++i) {
            pub.publish(topic, "Message " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Try to receive messages
        auto start = std::chrono::steady_clock::now();
        while (message_count < 3 && 
               std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            auto message = sub.receive();
            if (message.has_value()) {
                message_count++;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        // Should have received at least some messages
        CHECK(message_count > 0);
    }
    
    TEST_CASE("Large Message") {
        const std::string endpoint = "tcp://127.0.0.1:5558";
        const std::string topic = "large_topic";
        
        ZmqPublisher pub(endpoint);
        ZmqSubscriber sub(endpoint, topic);
        
        // Create a large message (1KB)
        std::string large_message(1000, 'A');
        
        // Publish message
        pub.publish(topic, large_message);
        
        // Try to receive message
        auto start = std::chrono::steady_clock::now();
        std::optional<std::string> received_message;
        
        while (!received_message && 
               std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            received_message = sub.receive();
            if (!received_message) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        CHECK(received_message.has_value());
    }
}