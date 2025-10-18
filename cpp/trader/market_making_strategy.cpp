#include "market_making_strategy.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

MarketMakingStrategy::MarketMakingStrategy(
    const std::string& symbol,
    std::shared_ptr<GlftTarget> glft_model,
    const std::string& md_endpoint,
    const std::string& md_topic,
    const std::string& pos_endpoint,
    const std::string& pos_topic,
    const std::string& inventory_endpoint,
    const std::string& inventory_topic)
  : symbol_(symbol)
  , glft_model_(glft_model) {
  
  // Initialize OMS components
  oms_ = std::make_unique<OMS>();
  
  // Set up OMS event callback to forward events to strategy callbacks
  oms_->on_event = [this](const OrderEvent& event) {
    if (order_event_callback_) {
      order_event_callback_(event);
    }
  };
  
  // Initialize market data subscriber if endpoint provided
  if (!md_endpoint.empty()) {
    md_subscriber_ = std::make_unique<ZmqSubscriber>(md_endpoint, md_topic);
    enable_market_data_ = true;
  }
  
  // Initialize position subscriber if endpoint provided
  if (!pos_endpoint.empty()) {
    pos_subscriber_ = std::make_unique<ZmqSubscriber>(pos_endpoint, pos_topic);
    enable_positions_ = true;
  }
  
  // Initialize inventory subscriber if endpoint provided (from DeFi)
  if (!inventory_endpoint.empty()) {
    inventory_subscriber_ = std::make_unique<ZmqSubscriber>(inventory_endpoint, inventory_topic);
    enable_inventory_ = true;
  }
  
  std::cout << "[MARKET_MAKING_STRATEGY] Initialized for symbol: " << symbol_ << std::endl;
}

MarketMakingStrategy::~MarketMakingStrategy() {
  stop();
}

void MarketMakingStrategy::start() {
  if (running_.load()) return;
  
  running_.store(true);
  
  // Connect to all registered exchanges
  // Note: Simple OMS doesn't have connect_all_exchanges method
  // Exchanges are connected individually when registered
  
  // Start processing threads
  if (enable_market_data_) {
    md_thread_ = std::thread([this]() { process_market_data(); });
  }
  
  if (enable_positions_) {
    pos_thread_ = std::thread([this]() { process_position_data(); });
  }
  
  if (enable_inventory_) {
    inventory_thread_ = std::thread([this]() { process_inventory_data(); });
  }
  
  order_thread_ = std::thread([this]() { process_order_events(); });
  
  std::cout << "[MARKET_MAKING_STRATEGY] Started for " << symbol_ << std::endl;
}

void MarketMakingStrategy::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  // Disconnect from exchanges
  disconnect_from_exchanges();
  
  // Wait for threads to finish (with timeout)
  std::cout << "[MARKET_MAKING_STRATEGY] Waiting for threads to finish..." << std::endl;
  
  if (md_thread_.joinable()) {
    md_thread_.join();
    std::cout << "[MARKET_MAKING_STRATEGY] Market data thread stopped" << std::endl;
  }
  if (pos_thread_.joinable()) {
    pos_thread_.join();
    std::cout << "[MARKET_MAKING_STRATEGY] Position thread stopped" << std::endl;
  }
  if (inventory_thread_.joinable()) {
    inventory_thread_.join();
    std::cout << "[MARKET_MAKING_STRATEGY] Inventory thread stopped" << std::endl;
  }
  if (order_thread_.joinable()) {
    order_thread_.join();
    std::cout << "[MARKET_MAKING_STRATEGY] Order thread stopped" << std::endl;
  }
  
  std::cout << "[MARKET_MAKING_STRATEGY] Stopped for " << symbol_ << std::endl;
}

void MarketMakingStrategy::on_orderbook_update(
    const std::string& symbol,
    const std::vector<std::pair<double, double>>& bids,
    const std::vector<std::pair<double, double>>& asks,
    uint64_t timestamp_us) {
  
  if (symbol != symbol_) return;
  
  if (bids.empty() || asks.empty()) return;
  
  double best_bid = bids[0].first;
  double best_ask = asks[0].first;
  double mid_price = (best_bid + best_ask) / 2.0;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Orderbook update: " << symbol 
            << " bid=" << best_bid << " ask=" << best_ask << " mid=" << mid_price << std::endl;
  
  // Update quotes using GLFT model
  update_quotes();
}

void MarketMakingStrategy::on_position_update(
    const std::string& symbol,
    const std::string& exch,
    double qty,
    double avg_price) {
  
  if (symbol != symbol_) return;
  
  current_positions_[exch] = qty;
  avg_prices_[exch] = avg_price;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Position update: " << symbol 
            << " " << exch << " qty=" << qty << " avg_price=" << avg_price << std::endl;
  
  // Update quotes based on new position
  update_quotes();
}

void MarketMakingStrategy::on_inventory_update(
    const std::string& symbol,
    double inventory_delta) {
  
  if (symbol != symbol_) return;
  
  current_inventory_delta_.store(inventory_delta);
  
  std::cout << "[MARKET_MAKING_STRATEGY] Inventory update: " << symbol 
            << " delta=" << inventory_delta << std::endl;
  
  // Update quotes based on inventory delta
  update_quotes();
}

void MarketMakingStrategy::submit_order(const Order& order) {
  std::cout << "[MARKET_MAKING_STRATEGY] Submitting order: " << order.cl_ord_id 
            << " " << to_string(order.side) << " " << order.qty 
            << " " << symbol_ << " @ " << order.price << std::endl;
  
  oms_->send(order);
}

void MarketMakingStrategy::cancel_order(const std::string& cl_ord_id) {
  std::cout << "[MARKET_MAKING_STRATEGY] Cancelling order: " << cl_ord_id << std::endl;
  
  // Find the first available exchange to cancel the order
  // TODO: Track which exchange the order was sent to
  if (!exchange_oms_.empty()) {
    std::string exchange_name = exchange_oms_.begin()->first;
    oms_->cancel(exchange_name, cl_ord_id);
  } else {
    std::cout << "[MARKET_MAKING_STRATEGY] No exchanges available for order cancellation" << std::endl;
  }
}

void MarketMakingStrategy::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
  std::cout << "[MARKET_MAKING_STRATEGY] Modifying order: " << cl_ord_id 
            << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
  
  // Find the first available exchange to modify the order
  // TODO: Track which exchange the order was sent to
  if (!exchange_oms_.empty()) {
    std::string exchange_name = exchange_oms_.begin()->first;
    // Note: Simple OMS doesn't support order modification
    // For now, cancel the old order and submit a new one
    oms_->cancel(exchange_name, cl_ord_id);
    
    // Submit new order with modified parameters
    Order new_order;
    new_order.cl_ord_id = cl_ord_id + "_MODIFIED";
    new_order.exch = exchange_name;
    new_order.symbol = symbol_;
    new_order.side = Side::Buy; // TODO: Get from original order
    new_order.qty = new_qty;
    new_order.price = new_price;
    new_order.is_market = false;
    
    oms_->send(new_order);
  } else {
    std::cout << "[MARKET_MAKING_STRATEGY] No exchanges available for order modification" << std::endl;
  }
}

void MarketMakingStrategy::register_exchange(const std::string& exchange_name, std::shared_ptr<IExchangeOMS> oms) {
  std::cout << "[MARKET_MAKING_STRATEGY] Registering exchange: " << exchange_name << std::endl;
  
  exchange_oms_[exchange_name] = oms;
  this->oms_->register_exchange(exchange_name, oms);
}

void MarketMakingStrategy::disconnect_from_exchanges() {
  // Note: Simple IExchangeOMS doesn't have disconnect method
  // Connection management is handled by the exchange implementation
  for (auto& [exchange_name, oms] : exchange_oms_) {
    std::cout << "[MARKET_MAKING_STRATEGY] Disconnecting from " << exchange_name << std::endl;
    // TODO: Implement proper disconnection when exchange interface supports it
  }
}

OrderStateInfo MarketMakingStrategy::get_order_state(const std::string& cl_ord_id) {
  // TODO: Implement order state tracking
  OrderStateInfo empty_state;
  return empty_state;
}

std::vector<OrderStateInfo> MarketMakingStrategy::get_active_orders() {
  // TODO: Implement active order tracking
  return {};
}

std::vector<OrderStateInfo> MarketMakingStrategy::get_all_orders() {
  // TODO: Implement order history tracking
  return {};
}

MarketMakingStrategy::OrderStats MarketMakingStrategy::get_order_statistics() {
  // TODO: Implement order statistics tracking
  OrderStats stats;
  return stats;
}

void MarketMakingStrategy::on_message(const std::string& handler_name, const std::string& data) {
  std::cout << "[MARKET_MAKING_STRATEGY] Received message from handler '" << handler_name 
            << "': " << data.substr(0, std::min(data.length(), size_t(100))) 
            << (data.length() > 100 ? "..." : "") << std::endl;
  
  // Route messages based on handler name
  if (handler_name == "market_data") {
    // TODO: Parse binary orderbook data
  } else if (handler_name == "positions") {
    // TODO: Parse position data
  } else if (handler_name == "inventory") {
    // TODO: Parse inventory delta
  } else if (handler_name == "order_events") {
    // TODO: Parse order event data
  } else {
    std::cout << "[MARKET_MAKING_STRATEGY] Unknown handler: " << handler_name << std::endl;
  }
}

void MarketMakingStrategy::process_market_data() {
  if (!md_subscriber_) return;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Market data processor started" << std::endl;
  
  while (running_.load()) {
    try {
      auto message = md_subscriber_->receive();
      if (message.has_value()) {
        // TODO: Parse market data message and call on_orderbook_update
        // For now, simulate with mock data
        static int counter = 0;
        if (counter % 10 == 0) { // Every 10th message
          std::vector<std::pair<double, double>> bids = {{50000.0, 0.1}};
          std::vector<std::pair<double, double>> asks = {{50001.0, 0.1}};
          on_orderbook_update(symbol_, bids, asks, 
            std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count());
        }
        counter++;
      }
    } catch (const std::exception& e) {
      std::cout << "[MARKET_MAKING_STRATEGY] Error processing market data: " << e.what() << std::endl;
    }
  }
}

void MarketMakingStrategy::process_position_data() {
  if (!pos_subscriber_) return;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Position data processor started" << std::endl;
  
  while (running_.load()) {
    try {
      auto message = pos_subscriber_->receive();
      if (message.has_value()) {
        // TODO: Parse position data message and call on_position_update
        // For now, simulate with mock data
        static int counter = 0;
        if (counter % 20 == 0) { // Every 20th message
          on_position_update(symbol_, "BINANCE", 0.5, 50000.0);
        }
        counter++;
      }
    } catch (const std::exception& e) {
      std::cout << "[MARKET_MAKING_STRATEGY] Error processing position data: " << e.what() << std::endl;
    }
  }
}

void MarketMakingStrategy::process_inventory_data() {
  if (!inventory_subscriber_) return;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Inventory data processor started" << std::endl;
  
  while (running_.load()) {
    try {
      auto message = inventory_subscriber_->receive();
      if (message.has_value()) {
        // TODO: Parse inventory data message and call on_inventory_update
        // For now, simulate with mock data
        static int counter = 0;
        if (counter % 30 == 0) { // Every 30th message
          double inventory_delta = (counter % 60 < 30) ? 0.1 : -0.1; // Oscillating inventory
          on_inventory_update(symbol_, inventory_delta);
        }
        counter++;
      }
    } catch (const std::exception& e) {
      std::cout << "[MARKET_MAKING_STRATEGY] Error processing inventory data: " << e.what() << std::endl;
    }
  }
}

void MarketMakingStrategy::process_order_events() {
  std::cout << "[MARKET_MAKING_STRATEGY] Order event processor started" << std::endl;
  
  while (running_.load()) {
    try {
      // Process order events from OMS
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } catch (const std::exception& e) {
      std::cout << "[MARKET_MAKING_STRATEGY] Error processing order events: " << e.what() << std::endl;
    }
  }
}

std::pair<double, double> MarketMakingStrategy::calculate_optimal_quotes(double mid_price, double inventory_delta) {
  // Use GLFT model for optimal pricing
  if (glft_model_) {
    // TODO: Integrate with GLFT model for optimal bid/ask calculation
    // For now, use simple inventory-based pricing
    double spread = mid_price * min_spread_bps_ / 10000.0; // Convert bps to decimal
    double inventory_adjustment = inventory_delta * 0.001; // Small adjustment based on inventory
    
    double bid_price = mid_price - spread/2.0 - inventory_adjustment;
    double ask_price = mid_price + spread/2.0 + inventory_adjustment;
    
    return {bid_price, ask_price};
  } else {
    // Fallback to simple spread
    double spread = mid_price * min_spread_bps_ / 10000.0;
    return {mid_price - spread/2.0, mid_price + spread/2.0};
  }
}

void MarketMakingStrategy::update_quotes() {
  // Cancel existing quotes
  cancel_existing_quotes();
  
  // Calculate optimal quotes using GLFT model
  double mid_price = 50000.0; // TODO: Get from market data
  double inventory_delta = current_inventory_delta_.load();
  
  auto [bid_price, ask_price] = calculate_optimal_quotes(mid_price, inventory_delta);
  
  // Submit new orders (simple strategy - use first exchange)
  std::string default_exchange = exchange_oms_.empty() ? "BINANCE" : exchange_oms_.begin()->first;
  
  Order bid_order;
  bid_order.cl_ord_id = "BID_" + symbol_ + "_" + std::to_string(std::time(nullptr));
  bid_order.exch = default_exchange;
  bid_order.symbol = symbol_;
  bid_order.side = Side::Buy;
  bid_order.qty = quote_size_;
  bid_order.price = bid_price;
  bid_order.is_market = false;
  
  Order ask_order;
  ask_order.cl_ord_id = "ASK_" + symbol_ + "_" + std::to_string(std::time(nullptr));
  ask_order.exch = default_exchange;
  ask_order.symbol = symbol_;
  ask_order.side = Side::Sell;
  ask_order.qty = quote_size_;
  ask_order.price = ask_price;
  ask_order.is_market = false;
  
  submit_order(bid_order);
  submit_order(ask_order);
  
  last_bid_order_id_ = bid_order.cl_ord_id;
  last_ask_order_id_ = ask_order.cl_ord_id;
  
  // Calculate spread in basis points
  double spread_bps = (ask_price - bid_price) / mid_price * 10000.0;
  
  std::cout << "[MARKET_MAKING_STRATEGY] Submitted quotes: bid=" << bid_price 
            << " ask=" << ask_price << " spread=" << std::fixed << std::setprecision(1) 
            << spread_bps << "bps across " << exchange_oms_.size() << " exchanges" << std::endl;
}

void MarketMakingStrategy::cancel_existing_quotes() {
  // Cancel existing quotes
  if (!last_bid_order_id_.empty()) {
    cancel_order(last_bid_order_id_);
  }
  if (!last_ask_order_id_.empty()) {
    cancel_order(last_ask_order_id_);
  }
}