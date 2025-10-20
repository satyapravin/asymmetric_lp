#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include "../utils/zmq/zmq_subscriber.hpp"
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
    std::cout << "[PMS_ADAPTER] Stopping PMS adapter" << std::endl;
    running_.store(false);
    
    // Close ZMQ subscriber to unblock receive() call
    if (subscriber_) {
      subscriber_.reset();
      std::cout << "[PMS_ADAPTER] ZMQ subscriber closed" << std::endl;
    }
    
    if (worker_.joinable()) {
      worker_.join();
      std::cout << "[PMS_ADAPTER] PMS adapter stopped" << std::endl;
    }
  }

  void set_position_callback(PositionUpdateCallback callback) {
    position_callback_ = callback;
    std::cout << "[PMS_ADAPTER] Position callback set: " << (callback ? "YES" : "NO") << std::endl;
  }

private:
  void run() {
    std::cout << "[PMS_ADAPTER] Starting to listen on " << endpoint_ << " topic: " << topic_ << std::endl;
    subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
    while (running_.load()) {
      auto msg = subscriber_->receive();
      if (!msg) continue;
      
      std::cout << "[PMS_ADAPTER] Received message of size: " << msg->size() << " bytes" << std::endl;
      
      // Parse protobuf position update
      proto::PositionUpdate position;
      if (position.ParseFromString(*msg)) {
        std::cout << "[PMS_ADAPTER] Parsed protobuf: " << position.symbol() << " qty: " << position.qty() << std::endl;
        if (position_callback_) {
          std::cout << "[PMS_ADAPTER] Calling position callback" << std::endl;
          position_callback_(position);
        }
      } else {
        std::cout << "[PMS_ADAPTER] Failed to parse protobuf message" << std::endl;
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
