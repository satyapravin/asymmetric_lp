#include "market_making_strategy.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

MarketMakingStrategy::MarketMakingStrategy(const std::string& symbol,
                                          const std::string& md_endpoint,
                                          const std::string& md_topic,
                                          std::shared_ptr<ZMQOMS> oms,
                                          std::shared_ptr<GlftTarget> glft_model)
    : symbol_(symbol), oms_(oms), glft_model_(glft_model) {
  md_subscriber_ = std::make_unique<ZmqSubscriber>(md_endpoint, md_topic);
}

MarketMakingStrategy::~MarketMakingStrategy() {
  stop();
}

void MarketMakingStrategy::start() {
  if (running_.load()) return;
  
  running_.store(true);
  strategy_thread_ = std::thread([this]() {
    while (running_.load()) {
      process_market_data();
      std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 100Hz
    }
  });
  
  std::cout << "[STRATEGY] Started market making for " << symbol_ << std::endl;
}

void MarketMakingStrategy::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  if (strategy_thread_.joinable()) {
    strategy_thread_.join();
  }
  
  cancel_existing_orders();
  std::cout << "[STRATEGY] Stopped market making for " << symbol_ << std::endl;
}

void MarketMakingStrategy::process_market_data() {
  auto msg = md_subscriber_->receive();
  if (!msg) return;
  
  if (msg->size() >= sizeof(OrderBookBinary)) {
    std::string symbol;
    std::vector<std::pair<double, double>> bids, asks;
    uint64_t timestamp_us;
    uint32_t sequence;
    
    if (OrderBookBinaryHelper::deserialize(*msg, symbol, bids, asks, timestamp_us, sequence)) {
      if (symbol == symbol_) {
        on_orderbook_update(symbol, bids, asks, timestamp_us);
      }
    }
  }
}

void MarketMakingStrategy::on_orderbook_update(const std::string& symbol,
                                              const std::vector<std::pair<double, double>>& bids,
                                              const std::vector<std::pair<double, double>>& asks,
                                              uint64_t timestamp_us) {
  current_bids_ = bids;
  current_asks_ = asks;
  last_update_time_.store(timestamp_us);
  
  update_quotes();
}

void MarketMakingStrategy::update_quotes() {
  if (current_bids_.empty() || current_asks_.empty()) return;
  
  double best_bid = current_bids_[0].first;
  double best_ask = current_asks_[0].first;
  double mid_price = (best_bid + best_ask) / 2.0;
  
  // Calculate inventory-adjusted spread using GLFT model
  double inventory_delta = current_inventory_delta_.load();
  double target_inventory = glft_model_->compute_target(-inventory_delta);
  double inventory_skew = inventory_delta - target_inventory;
  
  // Base spread calculation (simplified GLFT logic)
  double base_spread_bps = min_spread_bps_;
  double inventory_component = std::abs(inventory_skew) * 10.0; // 10 bps per unit skew
  double total_spread_bps = base_spread_bps + inventory_component;
  
  // Clamp spread
  total_spread_bps = std::max(total_spread_bps, min_spread_bps_);
  total_spread_bps = std::min(total_spread_bps, 100.0); // Max 100 bps
  
  double spread = mid_price * total_spread_bps / 10000.0;
  
  // Adjust quotes based on inventory
  double bid_price = mid_price - spread * 0.5;
  double ask_price = mid_price + spread * 0.5;
  
  // Inventory skewing: widen quotes on the side we want to reduce
  if (inventory_skew > 0) {
    // Long inventory, widen ask (make it harder to sell)
    ask_price += spread * 0.5;
  } else if (inventory_skew < 0) {
    // Short inventory, widen bid (make it harder to buy)
    bid_price -= spread * 0.5;
  }
  
  // Position size limits
  double current_position = inventory_delta;
  if (std::abs(current_position) >= max_position_size_) {
    // Stop quoting on the side that would increase position
    if (current_position > 0) {
      ask_price = 0; // Don't quote ask
    } else {
      bid_price = 0; // Don't quote bid
    }
  }
  
  place_new_quotes(mid_price, spread);
}

void MarketMakingStrategy::cancel_existing_orders() {
  if (has_active_bid_.load() && !active_bid_order_id_.empty()) {
    oms_->cancel_order(active_bid_order_id_, "GRVT");
    has_active_bid_.store(false);
    active_bid_order_id_.clear();
  }
  
  if (has_active_ask_.load() && !active_ask_order_id_.empty()) {
    oms_->cancel_order(active_ask_order_id_, "GRVT");
    has_active_ask_.store(false);
    active_ask_order_id_.clear();
  }
}

void MarketMakingStrategy::place_new_quotes(double mid_price, double spread) {
  cancel_existing_orders();
  
  double best_bid = current_bids_.empty() ? mid_price - spread : current_bids_[0].first;
  double best_ask = current_asks_.empty() ? mid_price + spread : current_asks_[0].first;
  
  // Place bid quote (if we have room and want to buy)
  double bid_price = mid_price - spread * 0.5;
  if (bid_price > best_bid && std::abs(current_inventory_delta_.load()) < max_position_size_) {
    std::string bid_id = symbol_ + "-BID-" + std::to_string(std::time(nullptr));
    if (oms_->send_order(bid_id, "GRVT", symbol_, 0, 0, quote_size_, bid_price)) {
      active_bid_order_id_ = bid_id;
      has_active_bid_.store(true);
      std::cout << "[STRATEGY] Placed bid: " << bid_price << " @ " << quote_size_ << std::endl;
    }
  }
  
  // Place ask quote (if we have room and want to sell)
  double ask_price = mid_price + spread * 0.5;
  if (ask_price < best_ask && std::abs(current_inventory_delta_.load()) < max_position_size_) {
    std::string ask_id = symbol_ + "-ASK-" + std::to_string(std::time(nullptr));
    if (oms_->send_order(ask_id, "GRVT", symbol_, 1, 0, quote_size_, ask_price)) {
      active_ask_order_id_ = ask_id;
      has_active_ask_.store(true);
      std::cout << "[STRATEGY] Placed ask: " << ask_price << " @ " << quote_size_ << std::endl;
    }
  }
}
