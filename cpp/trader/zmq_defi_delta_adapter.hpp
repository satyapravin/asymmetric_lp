#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/logging/log_helper.hpp"

// DeFi Delta ZMQ Adapter
// Connects trader to Python LP rebalancer via ZMQ to receive DeFi inventory deltas
class ZmqDefiDeltaAdapter {
public:
  using DefiDeltaCallback = std::function<void(const std::string& asset_symbol, double delta_tokens)>;
  
  ZmqDefiDeltaAdapter(const std::string& endpoint, const std::string& topic)
      : endpoint_(endpoint), topic_(topic) {
    running_.store(true);
    worker_ = std::thread([this]() { this->run(); });
  }
  
  void set_delta_callback(DefiDeltaCallback callback) {
    delta_callback_ = callback;
    LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "Delta callback set: " + std::string(callback ? "YES" : "NO"));
  }

  ~ZmqDefiDeltaAdapter() {
    stop();
  }

  void stop() {
    LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "Stopping DeFi delta adapter");
    running_.store(false);
    
    // Close ZMQ subscriber to unblock receive() calls
    if (subscriber_) {
      subscriber_.reset();
      LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "DeFi delta ZMQ subscriber closed");
    }
    
    if (worker_.joinable()) {
      worker_.join();
      LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "DeFi delta worker stopped");
    }
    LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "DeFi delta adapter stopped");
  }

private:
  void run() {
    LOG_INFO_COMP("DEFI_DELTA_ADAPTER", "Starting to listen on " + endpoint_ + " topic: " + topic_);
    // Python sends single string messages: "inventory_update {json}"
    // Subscribe to topic prefix to filter, but we'll parse the full message
    subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
    
    while (running_.load()) {
      auto msg = subscriber_->receive();
      if (!msg) continue;
      
      LOG_DEBUG_COMP("DEFI_DELTA_ADAPTER", "Received message of size: " + std::to_string(msg->size()) + " bytes");
      
      // Python publishes messages as single string: "inventory_update {json}"
      // ZmqSubscriber filters by topic, but Python sends topic+payload as single string
      // So we receive the full string and need to extract JSON payload
      std::string message_str = *msg;
      
      // Check if message starts with our topic (should always be true due to ZMQ filtering)
      size_t topic_pos = message_str.find(topic_ + " ");
      if (topic_pos != 0) {
        // Try to find topic anywhere in message (Python might send differently)
        topic_pos = message_str.find(topic_);
        if (topic_pos == std::string::npos) {
          LOG_DEBUG_COMP("DEFI_DELTA_ADAPTER", "Message doesn't contain expected topic: " + message_str.substr(0, 100));
          continue;
        }
        // Skip past topic and any whitespace
        size_t json_start = message_str.find_first_not_of(" \t", topic_pos + topic_.length());
        if (json_start == std::string::npos) {
          LOG_WARN_COMP("DEFI_DELTA_ADAPTER", "No JSON payload found after topic");
          continue;
        }
        message_str = message_str.substr(json_start);
      } else {
        // Extract JSON payload (after topic and space)
        size_t space_pos = message_str.find(' ');
        if (space_pos == std::string::npos) {
          LOG_WARN_COMP("DEFI_DELTA_ADAPTER", "Invalid message format (no space separator): " + message_str.substr(0, 100));
          continue;
        }
        message_str = message_str.substr(space_pos + 1);
      }
      
      // Parse JSON delta message
      auto delta_msg = ZmqSubscriber::parse_minimal_delta(message_str);
      if (!delta_msg.has_value()) {
        LOG_WARN_COMP("DEFI_DELTA_ADAPTER", "Failed to parse delta message JSON: " + message_str.substr(0, 200));
        continue;
      }
      
      LOG_DEBUG_COMP("DEFI_DELTA_ADAPTER", 
                    "Parsed delta: asset_symbol=" + delta_msg->asset_symbol + 
                    " delta_units=" + std::to_string(delta_msg->delta_units));
      
      // Forward to callback (delta_units is in tokens)
      if (delta_callback_) {
        delta_callback_(delta_msg->asset_symbol, delta_msg->delta_units);
      }
    }
  }

  std::string endpoint_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::unique_ptr<ZmqSubscriber> subscriber_;
  DefiDeltaCallback delta_callback_;
};

