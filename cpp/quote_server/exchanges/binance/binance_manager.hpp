#pragma once
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <simdjson.h>
#include "../common/exchange_manager_base.hpp"

class BinanceManager : public ExchangeManagerBase {
public:
  explicit BinanceManager(const std::string& websocket_url);
  ~BinanceManager();

  bool start() override;
  void stop() override;

  void subscribe_symbol(const std::string& symbol) override;
  void unsubscribe_symbol(const std::string& symbol) override;
  void configure_kv(const std::vector<std::pair<std::string, std::string>>& kv) override {
    for (const auto& [k, v] : kv) {
      if (k == "WEBSOCKET_URL") websocket_url_ = v;
      else if (k == "CHANNEL") { channels_.push_back(v); }
      else if (k == "SYMBOL") {
        subscribe_symbol(v);
      } else if (k == "BOOK_DEPTH") {
        try { book_depth_ = std::stoi(v); } catch (...) {}
      }
    }
  }

  // Public wrappers to allow callbacks to signal events
  void handle_ws_open();
  void handle_ws_close();
  void handle_ws_message(const std::string& msg);

private:
  void process_complete_message(const std::string& message);
  void process_orderbook_message(const simdjson::dom::object& data, 
                                const std::string& symbol, 
                                uint64_t timestamp_us);
  void process_trade_message(const simdjson::dom::object& data, 
                            const std::string& symbol, 
                            uint64_t timestamp_us);

private:
  std::string websocket_url_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::string message_buffer_; // Buffer for fragmented messages
  volatile int runflag_{0};
  std::vector<std::string> subs_;
  bool snapshot_only_{true};
  int book_depth_{50};
  std::vector<std::string> channels_;
  std::mutex subs_mutex_;
};

extern "C" {
  BinanceManager* create_exchange_manager(const char* websocket_url);
  void destroy_exchange_manager(BinanceManager* mgr);
}


