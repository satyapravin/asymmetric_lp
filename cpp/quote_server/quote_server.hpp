#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <chrono>
#include <functional>
#include "../utils/mds/orderbook_binary.hpp"
#include "../utils/mds/market_data_normalizer.hpp"
#include "../utils/mds/parser_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"

// Forward declaration
class IExchangeManager;

// Quote server framework for managing market data feeds
class QuoteServer {
public:
  using SymbolCallback = std::function<void(const std::string& symbol,
                                          const std::vector<std::pair<double, double>>& bids,
                                          const std::vector<std::pair<double, double>>& asks,
                                          uint64_t timestamp_us)>;
  
  QuoteServer(const std::string& exchange_name, const std::string& zmq_endpoint);
  ~QuoteServer();
  
  // Lifecycle
  void start();
  void stop();
  bool is_running() const { return running_.load(); }
  
  // Exchange connection management
  void set_websocket_url(const std::string& url);
  void connect_to_exchange();
  void disconnect_from_exchange();
  
  // Symbol management
  void add_symbol(const std::string& symbol);
  void remove_symbol(const std::string& symbol);
  std::vector<std::string> get_active_symbols() const;
  
  // Data processing
  void process_raw_message(const std::string& raw_msg);
  void process_orderbook_update(const std::string& symbol,
                               const std::vector<std::pair<double, double>>& bids,
                               const std::vector<std::pair<double, double>>& asks,
                               uint64_t timestamp_us);
  
  // Configuration
  void set_parser(std::unique_ptr<IExchangeParser> parser);
  void set_publish_rate_hz(double rate_hz) { publish_rate_hz_ = rate_hz; }
  void set_max_depth(int depth) { max_depth_ = depth; }
  // Provide raw exchange-specific config entries for the manager
  void set_exchange_config(const std::vector<std::pair<std::string, std::string>>& kv) { exchange_kv_ = kv; }
  
  // Statistics
  struct Stats {
    uint64_t messages_processed{0};
    uint64_t orderbooks_published{0};
    uint64_t parse_errors{0};
    uint64_t last_update_time_us{0};
  };
  
  Stats get_stats() const;
  void reset_stats();

private:
  void publish_loop();
  void publish_orderbook(const std::string& symbol,
                        const std::vector<std::pair<double, double>>& bids,
                        const std::vector<std::pair<double, double>>& asks,
                        uint64_t timestamp_us);
  
  std::string exchange_name_;
  std::string zmq_endpoint_;
  std::string websocket_url_;
  std::unique_ptr<ZmqPublisher> publisher_;
  std::unique_ptr<MarketDataNormalizer> normalizer_;
  std::unique_ptr<IExchangeManager> exchange_manager_;
  std::vector<std::pair<std::string, std::string>> exchange_kv_;
  
  // Symbol management
  std::set<std::string> active_symbols_;
  mutable std::mutex symbols_mutex_;
  
  // Publishing
  std::atomic<bool> running_{false};
  std::thread publish_thread_;
  double publish_rate_hz_{20.0}; // 20Hz default
  int max_depth_{10}; // Max orderbook depth
  
  // Statistics
  mutable std::mutex stats_mutex_;
  Stats stats_;
};
