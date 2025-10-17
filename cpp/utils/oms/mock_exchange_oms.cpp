#include "mock_exchange_oms.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

MockExchangeOMS::MockExchangeOMS(const std::string& exchange_name, 
                               double fill_probability,
                               double reject_probability,
                               std::chrono::milliseconds response_delay)
  : exchange_name_(exchange_name)
  , fill_probability_(fill_probability)
  , reject_probability_(reject_probability)
  , response_delay_(response_delay)
  , rng_(std::random_device{}())
  , prob_dist_(0.0, 1.0) {
}

MockExchangeOMS::~MockExchangeOMS() {
  disconnect();
}

bool MockExchangeOMS::send_order(const Order& order) {
  if (!connected_.load()) {
    std::cout << "[" << exchange_name_ << "] Not connected, rejecting order " << order.cl_ord_id << std::endl;
    return false;
  }
  
  std::cout << "[" << exchange_name_ << "] Received order: " << order.cl_ord_id 
            << " " << to_string(order.side) << " " << order.qty 
            << " " << order.symbol << " @ " << order.price << std::endl;
  
  // Generate exchange order ID
  std::string exchange_order_id = generate_exchange_order_id();
  
  // Store order mapping
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    cl_ord_to_exch_ord_[order.cl_ord_id] = exchange_order_id;
    active_orders_[exchange_order_id] = order;
  }
  
  // Simulate processing delay
  std::thread([this, order, exchange_order_id]() {
    std::this_thread::sleep_for(response_delay_);
    process_order(order);
  }).detach();
  
  return true;
}

bool MockExchangeOMS::cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) {
  if (!connected_.load()) {
    std::cout << "[" << exchange_name_ << "] Not connected, cannot cancel " << cl_ord_id << std::endl;
    return false;
  }
  
  std::string actual_exchange_order_id = exchange_order_id;
  
  // If no exchange order ID provided, look it up
  if (actual_exchange_order_id.empty()) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = cl_ord_to_exch_ord_.find(cl_ord_id);
    if (it == cl_ord_to_exch_ord_.end()) {
      std::cout << "[" << exchange_name_ << "] Order " << cl_ord_id << " not found" << std::endl;
      return false;
    }
    actual_exchange_order_id = it->second;
  }
  
  std::cout << "[" << exchange_name_ << "] Cancelling order " << cl_ord_id 
            << " (exchange ID: " << actual_exchange_order_id << ")" << std::endl;
  
  // Remove from active orders
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    cl_ord_to_exch_ord_.erase(cl_ord_id);
    active_orders_.erase(actual_exchange_order_id);
  }
  
  // Send cancel event
  if (on_order_event) {
    OrderEvent cancel_event;
    cancel_event.cl_ord_id = cl_ord_id;
    cancel_event.exch = exchange_name_;
    cancel_event.type = OrderEventType::Cancel;
    cancel_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    
    on_order_event(cancel_event);
  }
  
  return true;
}

bool MockExchangeOMS::modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                                  double new_price, double new_qty) {
  if (!connected_.load()) {
    std::cout << "[" << exchange_name_ << "] Not connected, cannot modify " << cl_ord_id << std::endl;
    return false;
  }
  
  std::string actual_exchange_order_id = exchange_order_id;
  
  // If no exchange order ID provided, look it up
  if (actual_exchange_order_id.empty()) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = cl_ord_to_exch_ord_.find(cl_ord_id);
    if (it == cl_ord_to_exch_ord_.end()) {
      std::cout << "[" << exchange_name_ << "] Order " << cl_ord_id << " not found" << std::endl;
      return false;
    }
    actual_exchange_order_id = it->second;
  }
  
  std::cout << "[" << exchange_name_ << "] Modifying order " << cl_ord_id 
            << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
  
  // Update order in active orders
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(actual_exchange_order_id);
    if (it != active_orders_.end()) {
      it->second.price = new_price;
      it->second.qty = new_qty;
    }
  }
  
  return true;
}

bool MockExchangeOMS::connect() {
  if (connected_.load()) return true;
  
  std::cout << "[" << exchange_name_ << "] Connecting..." << std::endl;
  
  // Simulate connection delay
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  connected_.store(true);
  running_.store(true);
  
  std::cout << "[" << exchange_name_ << "] Connected" << std::endl;
  return true;
}

void MockExchangeOMS::disconnect() {
  if (!connected_.load()) return;
  
  std::cout << "[" << exchange_name_ << "] Disconnecting..." << std::endl;
  
  running_.store(false);
  connected_.store(false);
  
  // Cancel all active orders
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    for (const auto& pair : cl_ord_to_exch_ord_) {
      const std::string& cl_ord_id = pair.first;
      
      if (on_order_event) {
        OrderEvent cancel_event;
        cancel_event.cl_ord_id = cl_ord_id;
        cancel_event.exch = exchange_name_;
        cancel_event.type = OrderEventType::Cancel;
        cancel_event.text = "Exchange disconnected";
        cancel_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        
        on_order_event(cancel_event);
      }
    }
    
    cl_ord_to_exch_ord_.clear();
    active_orders_.clear();
  }
  
  std::cout << "[" << exchange_name_ << "] Disconnected" << std::endl;
}

bool MockExchangeOMS::is_connected() const {
  return connected_.load();
}

std::vector<std::string> MockExchangeOMS::get_supported_symbols() const {
  return {"BTCUSDC-PERP", "ETHUSDC-PERP", "SOLUSDC-PERP", "ADAUSDC-PERP"};
}

void MockExchangeOMS::process_order(const Order& order) {
  std::string exchange_order_id;
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = cl_ord_to_exch_ord_.find(order.cl_ord_id);
    if (it == cl_ord_to_exch_ord_.end()) return;
    exchange_order_id = it->second;
  }
  
  // Simulate acknowledgment first
  simulate_ack(order, exchange_order_id);
  
  // Determine outcome based on probabilities
  double rand = prob_dist_(rng_);
  
  if (rand < reject_probability_) {
    simulate_reject(order, "Random rejection for testing");
  } else if (rand < reject_probability_ + fill_probability_) {
    simulate_fill(order, exchange_order_id);
  } else {
    // Order remains active (acknowledged but not filled)
    std::cout << "[" << exchange_name_ << "] Order " << order.cl_ord_id 
              << " acknowledged but not filled" << std::endl;
  }
}

void MockExchangeOMS::simulate_ack(const Order& order, const std::string& exchange_order_id) {
  std::cout << "[" << exchange_name_ << "] Acknowledging order " << order.cl_ord_id << std::endl;
  
  if (on_order_event) {
    OrderEvent ack_event;
    ack_event.cl_ord_id = order.cl_ord_id;
    ack_event.exch = exchange_name_;
    ack_event.symbol = order.symbol;
    ack_event.type = OrderEventType::Ack;
    ack_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    
    on_order_event(ack_event);
  }
}

void MockExchangeOMS::simulate_fill(const Order& order, const std::string& exchange_order_id) {
  std::cout << "[" << exchange_name_ << "] Filling order " << order.cl_ord_id << std::endl;
  
  // Simulate partial or full fill
  double fill_qty = order.qty;
  double fill_price = order.price;
  
  // Add some randomness to fill price (within 0.1% of order price)
  std::uniform_real_distribution<double> price_noise(-0.001, 0.001);
  fill_price *= (1.0 + price_noise(rng_));
  
  if (on_order_event) {
    OrderEvent fill_event;
    fill_event.cl_ord_id = order.cl_ord_id;
    fill_event.exch = exchange_name_;
    fill_event.symbol = order.symbol;
    fill_event.type = OrderEventType::Fill;
    fill_event.fill_qty = fill_qty;
    fill_event.fill_price = fill_price;
    fill_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    
    on_order_event(fill_event);
  }
  
  // Remove from active orders
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    cl_ord_to_exch_ord_.erase(order.cl_ord_id);
    active_orders_.erase(exchange_order_id);
  }
}

void MockExchangeOMS::simulate_reject(const Order& order, const std::string& reason) {
  std::cout << "[" << exchange_name_ << "] Rejecting order " << order.cl_ord_id 
            << " reason: " << reason << std::endl;
  
  if (on_order_event) {
    OrderEvent reject_event;
    reject_event.cl_ord_id = order.cl_ord_id;
    reject_event.exch = exchange_name_;
    reject_event.symbol = order.symbol;
    reject_event.type = OrderEventType::Reject;
    reject_event.text = reason;
    reject_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    
    on_order_event(reject_event);
  }
  
  // Remove from active orders
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    cl_ord_to_exch_ord_.erase(order.cl_ord_id);
    active_orders_.erase(cl_ord_to_exch_ord_[order.cl_ord_id]);
  }
}

std::string MockExchangeOMS::generate_exchange_order_id() {
  std::ostringstream oss;
  oss << exchange_name_ << "_" << std::hex << std::setfill('0') << std::setw(8) 
      << (rng_() & 0xFFFFFFFF);
  return oss.str();
}
