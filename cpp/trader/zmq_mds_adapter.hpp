#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include "../utils/mds/market_data.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
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
    std::cout << "[MDS_ADAPTER] Stopping MDS adapter" << std::endl;
    running_.store(false);
    if (subscriber_) {
      subscriber_.reset(); // This will call the destructor and close the ZMQ socket
    }
    if (worker_.joinable()) {
      worker_.join();
      std::cout << "[MDS_ADAPTER] MDS adapter stopped" << std::endl;
    }
  }

  std::function<void(const proto::OrderBookSnapshot&)> on_snapshot;

private:
  void run() {
    subscriber_ = std::make_unique<ZmqSubscriber>(endpoint_, topic_);
    std::cout << "[MDS_ADAPTER] Starting to listen on " << endpoint_ << " topic: " << topic_ << std::endl;
    
    while (running_.load()) {
      auto msg = subscriber_->receive();
      if (!msg) continue;
      
      std::cout << "[MDS_ADAPTER] Received message of size: " << msg->size() << " bytes" << std::endl;
      
      // Deserialize protobuf OrderBookSnapshot
      proto::OrderBookSnapshot proto_orderbook;
      if (!proto_orderbook.ParseFromString(*msg)) {
        std::cerr << "[MDS_ADAPTER] Failed to parse protobuf message" << std::endl;
        continue;
      }
      
      std::cout << "[MDS_ADAPTER] Parsed protobuf: " << proto_orderbook.symbol() 
                << " bids: " << proto_orderbook.bids_size() << " asks: " << proto_orderbook.asks_size() << std::endl;
      
      if (on_snapshot) {
        std::cout << "[MDS_ADAPTER] Calling on_snapshot callback" << std::endl;
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
