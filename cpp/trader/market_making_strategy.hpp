#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <optional>
#include "../utils/mds/orderbook_binary.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "zmq_oms.hpp"
#include "models/glft_target.hpp"

// Market making strategy that reacts to market data updates
class MarketMakingStrategy {
public:
  using OrderBookCallback = std::function<void(const std::string& symbol,
                                             const std::vector<std::pair<double, double>>& bids,
                                             const std::vector<std::pair<double, double>>& asks,
                                             uint64_t timestamp_us)>;
  
  MarketMakingStrategy(const std::string& symbol,
                      const std::string& md_endpoint,
                      const std::string& md_topic,
                      std::shared_ptr<ZMQOMS> oms,
                      std::shared_ptr<GlftTarget> glft_model);
  
  ~MarketMakingStrategy();
  
  void start();
  void stop();
  
  // Configuration
  void set_inventory_delta(double delta) { current_inventory_delta_.store(delta); }
  void set_min_spread_bps(double bps) { min_spread_bps_ = bps; }
  void set_max_position_size(double size) { max_position_size_ = size; }
  void set_quote_size(double size) { quote_size_ = size; }
  
  // Market data callback
  void on_orderbook_update(const std::string& symbol,
                          const std::vector<std::pair<double, double>>& bids,
                          const std::vector<std::pair<double, double>>& asks,
                          uint64_t timestamp_us);

private:
  void process_market_data();
  void update_quotes();
  void cancel_existing_orders();
  void place_new_quotes(double mid_price, double spread);
  
  std::string symbol_;
  std::unique_ptr<ZmqSubscriber> md_subscriber_;
  std::shared_ptr<ZMQOMS> oms_;
  std::shared_ptr<GlftTarget> glft_model_;
  
  // Market data state
  std::atomic<bool> running_{false};
  std::vector<std::pair<double, double>> current_bids_;
  std::vector<std::pair<double, double>> current_asks_;
  std::atomic<uint64_t> last_update_time_{0};
  
  // Strategy state
  std::atomic<double> current_inventory_delta_{0.0};
  double min_spread_bps_{5.0};  // 5 basis points minimum spread
  double max_position_size_{100.0};
  double quote_size_{1.0};
  
  // Order tracking
  std::string active_bid_order_id_;
  std::string active_ask_order_id_;
  std::atomic<bool> has_active_bid_{false};
  std::atomic<bool> has_active_ask_{false};
  
  // Threading
  std::thread strategy_thread_;
};
