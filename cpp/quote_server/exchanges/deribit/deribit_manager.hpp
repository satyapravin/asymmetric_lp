#pragma once
#include "../common/exchange_manager_base.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

// Forward declaration for C-style callbacks
struct DeribitLwsCallbacks;

class DeribitManager : public ExchangeManagerBase {
public:
  DeribitManager(const std::string& websocket_url);
  ~DeribitManager() override;

  // IExchangeManager interface
  bool start() override;
  void stop() override;
  void subscribe_symbol(const std::string& symbol) override;
  void unsubscribe_symbol(const std::string& symbol) override;
  void configure_kv(const std::vector<std::pair<std::string, std::string>>& kv) override;

  // WebSocket event handlers
  void handle_ws_open();
  void handle_ws_close();
  void handle_ws_message(const std::string& message);

private:
  std::string websocket_url_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  volatile int runflag_{0};
  std::vector<std::string> subs_;
  bool snapshot_only_{true};
  int book_depth_{10};
  std::vector<std::string> channels_;
  std::mutex subs_mutex_;
  
  // Deribit-specific configuration
  std::string api_version_{"2.0"};
  uint32_t request_id_{1};
  
  void parse_config(const std::vector<std::pair<std::string, std::string>>& kv);
  std::string build_subscription_message();
  void process_orderbook_message(const std::string& message);
  void process_trade_message(const std::string& message);
};

// C-style callbacks for libwebsockets integration
extern "C" {
  DeribitManager* create_exchange_manager(const char* websocket_url);
  void destroy_exchange_manager(DeribitManager* mgr);
}