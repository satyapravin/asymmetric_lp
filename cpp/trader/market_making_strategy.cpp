#include "market_making_strategy.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <ctime>

#include "market_making_strategy.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <ctime>

MarketMakingStrategy::MarketMakingStrategy(const std::string& symbol,
                                          const std::string& md_endpoint,
                                          const std::string& md_topic,
                                          const std::string& pos_endpoint,
                                          const std::string& pos_topic,
                                          std::shared_ptr<ZMQOMS> oms,
                                          std::shared_ptr<GlftTarget> glft_model)
    : symbol_(symbol), oms_(oms), glft_model_(glft_model) {
  md_subscriber_ = std::make_unique<ZmqSubscriber>(md_endpoint, md_topic);
  pos_subscriber_ = std::make_unique<ZmqSubscriber>(pos_endpoint, pos_topic);
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
      process_position_data();
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
    
    if (OrderBookBinaryHelper::deserialize(msg->data(), msg->size(), symbol, bids, asks, timestamp_us, sequence)) {
      if (symbol == symbol_) {
        on_orderbook_update(symbol, bids, asks, timestamp_us);
      }
    }
  }
}

void MarketMakingStrategy::process_position_data() {
  auto msg = pos_subscriber_->receive();
  if (!msg) return;
  
  if (msg->size() >= sizeof(PositionBinary)) {
    std::string symbol, exch;
    double qty, avg_price;
    uint64_t timestamp_us;
    
    if (PositionBinaryHelper::deserialize_position(msg->data(), symbol, exch, qty, avg_price, timestamp_us)) {
      if (symbol == symbol_) {
        on_position_update(symbol, exch, qty, avg_price, timestamp_us);
      }
    }
  }
}

void MarketMakingStrategy::on_position_update(const std::string& symbol,
                                             const std::string& exch,
                                             double qty,
                                             double avg_price,
                                             uint64_t timestamp_us) {
  current_position_qty_.store(qty);
  current_avg_price_.store(avg_price);
  last_position_time_.store(timestamp_us);
  
  std::cout << "[POSITION] " << symbol << " " << exch 
            << " qty=" << qty << " avg_price=" << avg_price << std::endl;
  
  // Update quotes based on new position
  update_quotes();
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

void MarketMakingStrategy::on_order_event(const std::string& cl_ord_id,
                                         const std::string& exch,
                                         const std::string& symbol,
                                         uint32_t event_type,
                                         double fill_qty,
                                         double fill_price,
                                         const std::string& text) {
  std::cout << "[ORDER_EVENT] " << cl_ord_id << " " << symbol 
            << " type=" << event_type << " qty=" << fill_qty 
            << " price=" << fill_price << " " << text << std::endl;
  
  // Handle different order event types
  if (event_type == 1) { // Fill
    // Update active order tracking
    if (cl_ord_id == active_bid_order_id_) {
      has_active_bid_.store(false);
      active_bid_order_id_.clear();
    } else if (cl_ord_id == active_ask_order_id_) {
      has_active_ask_.store(false);
      active_ask_order_id_.clear();
    }
    
    // Update quotes after fill
    update_quotes();
  } else if (event_type == 2) { // Cancel
    // Update active order tracking
    if (cl_ord_id == active_bid_order_id_) {
      has_active_bid_.store(false);
      active_bid_order_id_.clear();
    } else if (cl_ord_id == active_ask_order_id_) {
      has_active_ask_.store(false);
      active_ask_order_id_.clear();
    }
  }
}

void MarketMakingStrategy::update_quotes() {
  if (current_bids_.empty() || current_asks_.empty()) return;
  
  double best_bid = current_bids_[0].first;
  double best_ask = current_asks_[0].first;
  double mid_price = (best_bid + best_ask) / 2.0;
  
  // Calculate inventory-adjusted spread using GLFT model
  double inventory_delta = current_inventory_delta_.load();
  double position_qty = current_position_qty_.load();
  
  // Combine LP inventory delta with exchange position
  double total_inventory = inventory_delta + position_qty;
  double target_inventory = glft_model_->compute_target(-total_inventory);
  double inventory_skew = total_inventory - target_inventory;
  
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
  if (std::abs(total_inventory) >= max_position_size_) {
    // Stop quoting on the side that would increase position
    if (total_inventory > 0) {
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
  double total_inventory = current_inventory_delta_.load() + current_position_qty_.load();
  if (bid_price > best_bid && std::abs(total_inventory) < max_position_size_) {
    std::string bid_id = symbol_ + "-BID-" + std::to_string(time(nullptr));
    if (oms_->send_order(bid_id, "GRVT", symbol_, 0, 0, quote_size_, bid_price)) {
      active_bid_order_id_ = bid_id;
      has_active_bid_.store(true);
      std::cout << "[STRATEGY] Placed bid: " << bid_price << " @ " << quote_size_ << std::endl;
    }
  }
  
  // Place ask quote (if we have room and want to sell)
  double ask_price = mid_price + spread * 0.5;
  if (ask_price < best_ask && std::abs(total_inventory) < max_position_size_) {
    std::string ask_id = symbol_ + "-ASK-" + std::to_string(time(nullptr));
    if (oms_->send_order(ask_id, "GRVT", symbol_, 1, 0, quote_size_, ask_price)) {
      active_ask_order_id_ = ask_id;
      has_active_ask_.store(true);
      std::cout << "[STRATEGY] Placed ask: " << ask_price << " @ " << quote_size_ << std::endl;
    }
  }
}
