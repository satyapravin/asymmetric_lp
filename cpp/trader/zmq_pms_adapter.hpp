#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/logging/log_helper.hpp"
#include "../proto/position.pb.h"

// Position Management System ZMQ Adapter
// Connects trader to Position Server via ZMQ
class ZmqPMSAdapter {
public:
  using PositionUpdateCallback = std::function<void(const proto::PositionUpdate& position)>;
  
  ZmqPMSAdapter(const std::string& endpoint, const std::string& topic)
      : endpoint_(endpoint), topic_(topic) {
    running_.store(true);
    worker_ = std::thread([this]() { this->run(); });
  }

  ~ZmqPMSAdapter() {
    stop();
  }

  void stop() {
    LOG_INFO_COMP("PMS_ADAPTER", "Stopping PMS adapter");
    running_.store(false);
    
    // Close ZMQ subscriber to unblock receive() call
    if (subscriber_) {
      subscriber_.reset();
      LOG_INFO_COMP("PMS_ADAPTER", "ZMQ subscriber closed");
    }
    
    if (worker_.joinable()) {
      worker_.join();
      LOG_INFO_COMP("PMS_ADAPTER", "PMS adapter stopped");
    }
  }

  void set_position_callback(PositionUpdateCallback callback) {
    position_callback_ = callback;
    LOG_INFO_COMP("PMS_ADAPTER", "Position callback set: " + std::string(callback ? "YES" : "NO"));
  }

private:
  void run() {
    LOG_INFO_COMP("PMS_ADAPTER", "Starting to listen on " + endpoint_ + " topic: " + topic_);
    subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
    while (running_.load()) {
      auto msg = subscriber_->receive();
      if (!msg) continue;
      
      LOG_DEBUG_COMP("PMS_ADAPTER", "Received message of size: " + std::to_string(msg->size()) + " bytes");
      
      // Parse protobuf position update
      proto::PositionUpdate position;
      if (position.ParseFromString(*msg)) {
        LOG_DEBUG_COMP("PMS_ADAPTER", "Parsed protobuf: " + position.symbol() + " qty: " + std::to_string(position.qty()));
        if (position_callback_) {
          LOG_DEBUG_COMP("PMS_ADAPTER", "Calling position callback");
          position_callback_(position);
        }
      } else {
        LOG_ERROR_COMP("PMS_ADAPTER", "Failed to parse protobuf message");
      }
    }
  }

  std::string endpoint_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::unique_ptr<ZmqSubscriber> subscriber_;
  PositionUpdateCallback position_callback_;
};
