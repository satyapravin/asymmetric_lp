#include "message_handler.hpp"
#include "../zmq/zmq_subscriber.hpp"
#include <iostream>
#include <chrono>

MessageHandler::MessageHandler(const std::string& name, 
                              const std::string& endpoint, 
                              const std::string& topic)
    : name_(name), endpoint_(endpoint), topic_(topic) {
  subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
  std::cout << "[MESSAGE_HANDLER] Created handler '" << name_ 
            << "' for topic '" << topic_ << "' at " << endpoint_ << std::endl;
}

MessageHandler::~MessageHandler() {
  stop();
}

void MessageHandler::start() {
  if (running_.load()) return;
  
  running_.store(true);
  handler_thread_ = std::make_unique<std::thread>([this]() {
    process_messages();
  });
  
  std::cout << "[MESSAGE_HANDLER] Started handler '" << name_ << "'" << std::endl;
}

void MessageHandler::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  if (handler_thread_ && handler_thread_->joinable()) {
    handler_thread_->join();
  }
  
  std::cout << "[MESSAGE_HANDLER] Stopped handler '" << name_ << "'" << std::endl;
}

void MessageHandler::process_messages() {
  while (running_.load()) {
    auto msg = subscriber_->receive();
    if (msg) {
      if (data_callback_) {
        data_callback_(name_, *msg);
      }
    } else {
      // No message received, sleep briefly to avoid busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}
