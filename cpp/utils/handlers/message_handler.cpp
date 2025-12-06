#include "message_handler.hpp"
#include "../zmq/zmq_subscriber.hpp"
#include "../logging/log_helper.hpp"
#include <chrono>

MessageHandler::MessageHandler(const std::string& name, 
                              const std::string& endpoint, 
                              const std::string& topic)
    : name_(name), endpoint_(endpoint), topic_(topic) {
  subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
  LOG_INFO_COMP("MESSAGE_HANDLER", "Created handler '" + name_ + 
                "' for topic '" + topic_ + "' at " + endpoint_);
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
  
  LOG_INFO_COMP("MESSAGE_HANDLER", "Started handler '" + name_ + "'");
}

void MessageHandler::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  if (handler_thread_ && handler_thread_->joinable()) {
    handler_thread_->join();
  }
  
  LOG_INFO_COMP("MESSAGE_HANDLER", "Stopped handler '" + name_ + "'");
}

void MessageHandler::process_messages() {
  while (running_.load()) {
    auto msg = subscriber_->receive_blocking(100); // 100ms timeout
    if (msg) {
      if (data_callback_) {
        data_callback_(name_, *msg);
      }
    }
    // No message received, continue loop (timeout handled by receive_blocking)
  }
}
