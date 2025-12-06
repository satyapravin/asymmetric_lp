#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include "../utils/mds/market_data.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/logging/log_helper.hpp"
#include "../proto/market_data.pb.h"

class ZmqMDSAdapter : public IExchangeMD {
public:
  ZmqMDSAdapter(const std::string& endpoint, const std::string& topic, const std::string& exch)
      : endpoint_(endpoint), topic_(topic), exch_(exch) {
    running_.store(true);
    worker_ = std::thread([this]() { this->run(); });
  }

  ~ZmqMDSAdapter() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
  }

  void subscribe(const std::string& symbol) override {
    (void)symbol;
  }

  void stop() {
    LOG_INFO_COMP("MDS_ADAPTER", "Stopping MDS adapter");
    running_.store(false);
    if (subscriber_) {
      subscriber_.reset(); // This will call the destructor and close the ZMQ socket
    }
    if (worker_.joinable()) {
      worker_.join();
      LOG_INFO_COMP("MDS_ADAPTER", "MDS adapter stopped");
    }
  }

  std::function<void(const proto::OrderBookSnapshot&)> on_snapshot;

private:
  void run() {
    subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
    LOG_INFO_COMP("MDS_ADAPTER", "Starting to listen on " + endpoint_ + " topic: " + topic_);
    
    while (running_.load()) {
      auto msg = subscriber_->receive();
      if (!msg) continue;
      
      LOG_INFO_COMP("MDS_ADAPTER", "Received message of size: " + std::to_string(msg->size()) + " bytes");
      
      // Deserialize protobuf OrderBookSnapshot
      proto::OrderBookSnapshot proto_orderbook;
      if (!proto_orderbook.ParseFromString(*msg)) {
        LOG_ERROR_COMP("MDS_ADAPTER", "Failed to parse protobuf message");
        continue;
      }
      
      std::string log_msg = "Parsed protobuf: " + proto_orderbook.symbol() + 
                            " bids: " + std::to_string(proto_orderbook.bids_size()) + 
                            " asks: " + std::to_string(proto_orderbook.asks_size());
      LOG_DEBUG_COMP("MDS_ADAPTER", log_msg);
      
      if (on_snapshot) {
        LOG_DEBUG_COMP("MDS_ADAPTER", "Calling on_snapshot callback");
        on_snapshot(proto_orderbook);
      }
    }
  }


  std::string endpoint_;
  std::string topic_;
  std::string exch_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::unique_ptr<ZmqSubscriber> subscriber_;
};
