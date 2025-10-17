#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include "../../../utils/pms/position_feed.hpp"

// Binance-specific position feed implementation
class BinancePositionFeed : public IExchangePositionFeed {
public:
  BinancePositionFeed(const std::string& api_key, const std::string& api_secret);
  ~BinancePositionFeed();
  
  bool connect(const std::string& account) override;
  void disconnect() override;
  bool is_connected() const override;
  
  // Configure symbols to track
  void add_symbol(const std::string& symbol);
  void remove_symbol(const std::string& symbol);

  // WebSocket handlers (public for C callbacks)
  void handle_ws_open();
  void handle_ws_close();
  void handle_ws_message(const std::string& message);

private:
  void run_websocket_client();
  void process_position_update(const std::string& message);
  std::string build_auth_message();
  
  std::string api_key_;
  std::string api_secret_;
  std::string websocket_url_;
  std::string account_;
  
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};
  std::thread worker_thread_;
  
  std::mutex symbols_mutex_;
  std::set<std::string> symbols_;
  
  // Position tracking
  std::mutex positions_mutex_;
  std::map<std::string, double> current_positions_;  // symbol -> qty
  std::map<std::string, double> avg_prices_;         // symbol -> avg_price
  
  // WebSocket state
  volatile int runflag_{0};
  std::string message_buffer_;
};
