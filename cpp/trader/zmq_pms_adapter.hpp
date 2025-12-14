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
#include "../proto/acc_balance.pb.h"

// Position Management System ZMQ Adapter
// Connects trader to Position Server via ZMQ
class ZmqPMSAdapter {
public:
  using PositionUpdateCallback = std::function<void(const proto::PositionUpdate& position)>;
  using BalanceUpdateCallback = std::function<void(const proto::AccountBalanceUpdate& balance)>;
  
  ZmqPMSAdapter(const std::string& endpoint, const std::string& topic)
      : endpoint_(endpoint), topic_(topic) {
    running_.store(true);
    worker_ = std::thread([this]() { this->run(); });
    // Start balance subscriber thread
    balance_worker_ = std::thread([this]() { this->run_balance_subscriber(); });
  }
  
  void set_balance_callback(BalanceUpdateCallback callback) {
    balance_callback_ = callback;
    LOG_INFO_COMP("PMS_ADAPTER", "Balance callback set: " + std::string(callback ? "YES" : "NO"));
  }

  ~ZmqPMSAdapter() {
    stop();
  }

  void stop() {
    LOG_INFO_COMP("PMS_ADAPTER", "Stopping PMS adapter");
    running_.store(false);
    
    // Join threads first - they will exit due to running_ being false and receive timeout
    if (worker_.joinable()) {
      worker_.join();
      LOG_INFO_COMP("PMS_ADAPTER", "Position worker stopped");
    }
    if (balance_worker_.joinable()) {
      balance_worker_.join();
      LOG_INFO_COMP("PMS_ADAPTER", "Balance worker stopped");
    }
    
    // Now safe to destroy subscribers after threads have exited
    if (subscriber_) {
      subscriber_.reset();
      LOG_INFO_COMP("PMS_ADAPTER", "Position ZMQ subscriber closed");
    }
    if (balance_subscriber_) {
      balance_subscriber_.reset();
      LOG_INFO_COMP("PMS_ADAPTER", "Balance ZMQ subscriber closed");
    }
    LOG_INFO_COMP("PMS_ADAPTER", "PMS adapter stopped");
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
      auto msg = subscriber_->receive_blocking(100); // 100ms timeout to allow checking running_ flag
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

  void run_balance_subscriber() {
    LOG_INFO_COMP("PMS_ADAPTER", "Starting balance subscriber on " + endpoint_ + " topic: balance_updates");
    balance_subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, "balance_updates");
    while (running_.load()) {
      auto msg = balance_subscriber_->receive_blocking(100); // 100ms timeout to allow checking running_ flag
      if (!msg) continue;
      
      LOG_DEBUG_COMP("PMS_ADAPTER", "Received balance message of size: " + std::to_string(msg->size()) + " bytes");
      
      // Parse protobuf balance update
      proto::AccountBalanceUpdate balance;
      if (balance.ParseFromString(*msg)) {
        LOG_DEBUG_COMP("PMS_ADAPTER", "Parsed balance update: " + std::to_string(balance.balances_size()) + " balances");
        if (balance_callback_) {
          LOG_DEBUG_COMP("PMS_ADAPTER", "Calling balance callback");
          balance_callback_(balance);
        }
      } else {
        LOG_ERROR_COMP("PMS_ADAPTER", "Failed to parse balance protobuf message");
      }
    }
  }

  std::string endpoint_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::thread balance_worker_;
  std::unique_ptr<ZmqSubscriber> subscriber_;
  std::unique_ptr<ZmqSubscriber> balance_subscriber_;  // Separate subscriber for balance updates
  PositionUpdateCallback position_callback_;
  BalanceUpdateCallback balance_callback_;
};
