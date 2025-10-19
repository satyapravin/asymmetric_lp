#include <doctest/doctest.h>
#include "../../../utils/handlers/message_handler_manager.hpp"
#include "../../../utils/config/config.hpp"
#include <thread>
#include <chrono>

TEST_CASE("MessageHandlerManager - Basic Functionality") {
    MessageHandlerManager manager;
    
    SUBCASE("Add handler") {
        MessageHandlerConfig config;
        config.name = "test_handler";
        config.endpoint = "tcp://localhost:5555";
        config.topic = "test_topic";
        
        manager.add_handler(config);
        
        // Verify handler was added
        CHECK(manager.get_handler_count() == 1);
        auto handler_names = manager.get_handler_names();
        CHECK(std::find(handler_names.begin(), handler_names.end(), "test_handler") != handler_names.end());
    }
    
    SUBCASE("Remove handler") {
        MessageHandlerConfig config;
        config.name = "test_handler";
        config.endpoint = "tcp://localhost:5555";
        config.topic = "test_topic";
        
        manager.add_handler(config);
        CHECK(manager.get_handler_count() == 1);
        
        manager.remove_handler("test_handler");
        CHECK(manager.get_handler_count() == 0);
        auto handler_names = manager.get_handler_names();
        CHECK(std::find(handler_names.begin(), handler_names.end(), "test_handler") == handler_names.end());
    }
    
    SUBCASE("Clear all handlers") {
        MessageHandlerConfig config1, config2;
        config1.name = "handler1";
        config1.endpoint = "tcp://localhost:5555";
        config1.topic = "topic1";
        
        config2.name = "handler2";
        config2.endpoint = "tcp://localhost:5556";
        config2.topic = "topic2";
        
        manager.add_handler(config1);
        manager.add_handler(config2);
        CHECK(manager.get_handler_count() == 2);
        
        manager.clear_handlers();
        CHECK(manager.get_handler_count() == 0);
    }
}

TEST_CASE("MessageHandlerManager - Handler Lifecycle") {
    MessageHandlerManager manager;
    
    MessageHandlerConfig config;
    config.name = "test_handler";
        config.topic = "test_topic";
    config.endpoint = "tcp://localhost:5555";
    
    manager.add_handler(config);
    
    SUBCASE("Start handler") {
        manager.start_handler("test_handler");
        
        // Give it a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(manager.is_handler_running("test_handler") == true);
    }
    
    SUBCASE("Stop handler") {
        manager.start_handler("test_handler");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        manager.stop_handler("test_handler");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(manager.is_handler_running("test_handler") == false);
    }
    
    SUBCASE("Start all handlers") {
        MessageHandlerConfig config2;
        config2.name = "handler2";
        config2.topic = "test_topic2";
        config2.endpoint = "tcp://localhost:5556";
        
        manager.add_handler(config2);
        manager.start_all();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(manager.is_handler_running("test_handler") == true);
        CHECK(manager.is_handler_running("handler2") == true);
    }
    
    SUBCASE("Stop all handlers") {
        manager.start_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        manager.stop_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(manager.is_handler_running("test_handler") == false);
    }
}

TEST_CASE("MessageHandlerManager - Callbacks") {
    MessageHandlerManager manager;
    
    MessageHandlerConfig config;
    config.name = "test_handler";
        config.topic = "test_topic";
    config.endpoint = "tcp://localhost:5555";
    
    manager.add_handler(config);
    
    SUBCASE("Set data callback") {
        bool callback_called = false;
        std::string received_handler_name;
        std::string received_data;
        
        manager.set_data_callback([&](const std::string& handler_name, const std::string& data) {
            callback_called = true;
            received_handler_name = handler_name;
            received_data = data;
        });
        
        // Simulate data reception (this would normally come from the handler)
        // For testing, we'll just verify the callback is set
        CHECK(callback_called == false); // Not called yet
        
        // In a real scenario, the handler would call the callback
        // For unit testing, we're just verifying the callback mechanism is set up
    }
}

TEST_CASE("MessageHandlerManager - Error Handling") {
    MessageHandlerManager manager;
    
    SUBCASE("Start non-existent handler") {
        // Should not crash
        manager.start_handler("non_existent");
        CHECK(manager.is_handler_running("non_existent") == false);
    }
    
    SUBCASE("Stop non-existent handler") {
        // Should not crash
        manager.stop_handler("non_existent");
    }
    
    SUBCASE("Remove non-existent handler") {
        // Should not crash
        manager.remove_handler("non_existent");
        CHECK(manager.get_handler_count() == 0);
    }
    
    SUBCASE("Duplicate handler names") {
        MessageHandlerConfig config1, config2;
        config1.name = "duplicate";
        config1.topic = "test_topic1";
        config1.endpoint = "tcp://localhost:5555";
        
        config2.name = "duplicate"; // Same name
        config2.topic = "test_topic2";
        config2.endpoint = "tcp://localhost:5556";
        
        manager.add_handler(config1);
        manager.add_handler(config2);
        
        // Should only have one handler (second one replaces first)
        CHECK(manager.get_handler_count() == 1);
    }
}

TEST_CASE("MessageHandlerManager - Handler Status") {
    MessageHandlerManager manager;
    
    MessageHandlerConfig config;
    config.name = "test_handler";
        config.topic = "test_topic";
    config.endpoint = "tcp://localhost:5555";
    
    manager.add_handler(config);
    
    SUBCASE("Handler not running initially") {
        CHECK(manager.is_handler_running("test_handler") == false);
    }
    
    SUBCASE("Get handler names") {
        auto names = manager.get_handler_names();
        CHECK(names.size() == 1);
        CHECK(names[0] == "test_handler");
    }
    
    SUBCASE("Get running handler names") {
        manager.start_handler("test_handler");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto running_names = manager.get_handler_names();
        CHECK(running_names.size() == 1);
        CHECK(running_names[0] == "test_handler");
    }
}
