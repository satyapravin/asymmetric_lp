#pragma once
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include "../common/exchange_manager_base.hpp"

class DeribitManager : public ExchangeManagerBase {
public:
  explicit DeribitManager(const std::string& websocket_url);
  ~DeribitManager();

  bool start() override;
  void stop() override;

  void subscribe_symbol(const std::string& symbol) override;
  void unsubscribe_symbol(const std::string& symbol) override;

private:
  void run_mock();

  std::string websocket_url_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::vector<std::string> subs_;
  bool snapshot_only_{true};
  int book_depth_{50};
};


