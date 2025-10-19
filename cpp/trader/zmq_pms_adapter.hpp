#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
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
    running_.store(false);
    if (worker_.joinable()) worker_.join();
  }

  void set_position_callback(PositionUpdateCallback callback) {
    position_callback_ = callback;
  }

private:
  void run() {
    ZmqSubscriber sub(endpoint_, topic_);
    while (running_.load()) {
      auto msg = sub.receive();
      if (!msg) continue;
      
      // Parse protobuf position update
      proto::PositionUpdate position;
      if (position.ParseFromString(*msg)) {
        if (position_callback_) {
          position_callback_(position);
        }
      }
    }
  }

  std::string endpoint_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  PositionUpdateCallback position_callback_;
};
