#include "oms.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

OrderManager::OrderManager(const std::string& request_endpoint, 
                         const std::string& event_endpoint)
  : request_subscriber_(request_endpoint, "order_requests")
  , event_publisher_(event_endpoint) {
}

OrderManager::~OrderManager() {
  stop();
}

bool OrderManager::submit_order(const Order& order) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  // Check if order already exists
  if (orders_.find(order.cl_ord_id) != orders_.end()) {
    std::cout << "[OMS] Order " << order.cl_ord_id << " already exists" << std::endl;
    return false;
  }
  
  // Create order state info
  OrderStateInfo order_info;
  order_info.cl_ord_id = order.cl_ord_id;
  order_info.exch = order.exch;
  order_info.symbol = order.symbol;
  order_info.side = order.side;
  order_info.qty = order.qty;
  order_info.price = order.price;
  order_info.is_market = order.is_market;
  order_info.state = OrderState::PENDING;
  
  orders_[order.cl_ord_id] = order_info;
  
  // Update statistics
  {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_orders++;
    stats_.total_volume += order.qty;
  }
  
  std::cout << "[OMS] Submitted order " << order.cl_ord_id 
            << " " << to_string(order.side) << " " << order.qty 
            << " " << order.symbol << " @ " << order.price << std::endl;
  
  emit_state_change(order_info);
  return true;
}

bool OrderManager::cancel_order(const std::string& cl_ord_id) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = orders_.find(cl_ord_id);
  if (it == orders_.end()) {
    std::cout << "[OMS] Order " << cl_ord_id << " not found" << std::endl;
    return false;
  }
  
  OrderStateInfo& order_info = it->second;
  
  // Check if order can be cancelled
  if (order_info.state == OrderState::FILLED || 
      order_info.state == OrderState::CANCELLED ||
      order_info.state == OrderState::REJECTED) {
    std::cout << "[OMS] Order " << cl_ord_id << " cannot be cancelled (state: " 
              << to_string(order_info.state) << ")" << std::endl;
    return false;
  }
  
  // Create cancel event
  OrderEvent cancel_event;
  cancel_event.cl_ord_id = cl_ord_id;
  cancel_event.exch = order_info.exch;
  cancel_event.symbol = order_info.symbol;
  cancel_event.type = OrderEventType::Cancel;
  cancel_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  
  update_order_state(cl_ord_id, OrderState::CANCELLED, cancel_event);
  
  std::cout << "[OMS] Cancelled order " << cl_ord_id << std::endl;
  return true;
}

bool OrderManager::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = orders_.find(cl_ord_id);
  if (it == orders_.end()) {
    std::cout << "[OMS] Order " << cl_ord_id << " not found" << std::endl;
    return false;
  }
  
  OrderStateInfo& order_info = it->second;
  
  // Check if order can be modified
  if (order_info.state != OrderState::ACKNOWLEDGED && 
      order_info.state != OrderState::PARTIALLY_FILLED) {
    std::cout << "[OMS] Order " << cl_ord_id << " cannot be modified (state: " 
              << to_string(order_info.state) << ")" << std::endl;
    return false;
  }
  
  // Update order parameters
  order_info.price = new_price;
  order_info.qty = new_qty;
  order_info.last_update_time = std::chrono::system_clock::now();
  
  std::cout << "[OMS] Modified order " << cl_ord_id 
            << " new_qty=" << new_qty << " new_price=" << new_price << std::endl;
  
  emit_state_change(order_info);
  return true;
}

OrderStateInfo OrderManager::get_order_state(const std::string& cl_ord_id) const {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = orders_.find(cl_ord_id);
  if (it == orders_.end()) {
    return OrderStateInfo{}; // Return empty state
  }
  
  return it->second;
}

std::vector<OrderStateInfo> OrderManager::get_active_orders() const {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  std::vector<OrderStateInfo> active_orders;
  for (const auto& pair : orders_) {
    const OrderStateInfo& order_info = pair.second;
    if (order_info.state == OrderState::PENDING ||
        order_info.state == OrderState::ACKNOWLEDGED ||
        order_info.state == OrderState::PARTIALLY_FILLED) {
      active_orders.push_back(order_info);
    }
  }
  
  return active_orders;
}

std::vector<OrderStateInfo> OrderManager::get_orders_by_symbol(const std::string& symbol) const {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  std::vector<OrderStateInfo> symbol_orders;
  for (const auto& pair : orders_) {
    if (pair.second.symbol == symbol) {
      symbol_orders.push_back(pair.second);
    }
  }
  
  return symbol_orders;
}

std::vector<OrderStateInfo> OrderManager::get_orders_by_exchange(const std::string& exch) const {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  std::vector<OrderStateInfo> exchange_orders;
  for (const auto& pair : orders_) {
    if (pair.second.exch == exch) {
      exchange_orders.push_back(pair.second);
    }
  }
  
  return exchange_orders;
}

OrderManager::OrderStats OrderManager::get_statistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void OrderManager::start() {
  if (running_.load()) return;
  
  running_.store(true);
  
  // Start processing threads
  request_thread_ = std::thread([this]() { process_order_requests(); });
  event_thread_ = std::thread([this]() { process_order_events(); });
  cleanup_thread_ = std::thread([this]() { cleanup_expired_orders(); });
  
  std::cout << "[OMS] Order Manager started" << std::endl;
}

void OrderManager::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  // Wait for threads to finish
  if (request_thread_.joinable()) request_thread_.join();
  if (event_thread_.joinable()) event_thread_.join();
  if (cleanup_thread_.joinable()) cleanup_thread_.join();
  
  std::cout << "[OMS] Order Manager stopped" << std::endl;
}

void OrderManager::process_order_requests() {
  std::cout << "[OMS] Order request processor started" << std::endl;
  
  while (running_.load()) {
    try {
      // TODO: Implement ZeroMQ message processing
      // For now, just sleep
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch (const std::exception& e) {
      std::cout << "[OMS] Error processing order requests: " << e.what() << std::endl;
    }
  }
}

void OrderManager::process_order_events() {
  std::cout << "[OMS] Order event processor started" << std::endl;
  
  while (running_.load()) {
    try {
      // TODO: Implement ZeroMQ event processing
      // For now, just sleep
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch (const std::exception& e) {
      std::cout << "[OMS] Error processing order events: " << e.what() << std::endl;
    }
  }
}

void OrderManager::cleanup_expired_orders() {
  std::cout << "[OMS] Order cleanup processor started" << std::endl;
  
  while (running_.load()) {
    try {
      std::this_thread::sleep_for(cleanup_interval_);
      
      auto now = std::chrono::system_clock::now();
      std::vector<std::string> expired_orders;
      
      {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        for (const auto& pair : orders_) {
          const OrderStateInfo& order_info = pair.second;
          auto age = now - order_info.created_time;
          
          if (age > order_timeout_ && 
              (order_info.state == OrderState::PENDING || 
               order_info.state == OrderState::ACKNOWLEDGED)) {
            expired_orders.push_back(pair.first);
          }
        }
      }
      
      // Mark expired orders
      for (const std::string& cl_ord_id : expired_orders) {
        OrderEvent expire_event;
        expire_event.cl_ord_id = cl_ord_id;
        expire_event.type = OrderEventType::Reject;
        expire_event.text = "Order expired";
        expire_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        
        update_order_state(cl_ord_id, OrderState::EXPIRED, expire_event);
        
        std::cout << "[OMS] Order " << cl_ord_id << " expired" << std::endl;
      }
      
    } catch (const std::exception& e) {
      std::cout << "[OMS] Error in cleanup: " << e.what() << std::endl;
    }
  }
}

void OrderManager::update_order_state(const std::string& cl_ord_id, OrderState new_state, 
                                     const OrderEvent& event) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = orders_.find(cl_ord_id);
  if (it == orders_.end()) return;
  
  OrderStateInfo& order_info = it->second;
  OrderState old_state = order_info.state;
  
  // Validate state transition
  if (!OrderStateMachine::isValidTransition(old_state, new_state)) {
    std::cout << "[OMS] Invalid state transition for " << cl_ord_id 
              << " from " << to_string(old_state) << " to " << to_string(new_state) << std::endl;
    return;
  }
  
  // Update order state
  order_info.state = new_state;
  order_info.last_update_time = std::chrono::system_clock::now();
  
  // Update fill information
  if (event.type == OrderEventType::Fill) {
    order_info.filled_qty += event.fill_qty;
    if (order_info.filled_qty > 0) {
      // Calculate weighted average fill price
      double total_value = order_info.avg_fill_price * (order_info.filled_qty - event.fill_qty) +
                          event.fill_price * event.fill_qty;
      order_info.avg_fill_price = total_value / order_info.filled_qty;
    }
  }
  
  // Update exchange order ID
  if (!event.exchange_order_id.empty()) {
    order_info.exchange_order_id = event.exchange_order_id;
  }
  
  // Update reject reason
  if (event.type == OrderEventType::Reject) {
    order_info.reject_reason = event.text;
  }
  
  // Update statistics
  {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    switch (new_state) {
      case OrderState::FILLED:
        stats_.filled_orders++;
        stats_.total_filled_volume += order_info.filled_qty;
        break;
      case OrderState::CANCELLED:
        stats_.cancelled_orders++;
        break;
      case OrderState::REJECTED:
        stats_.rejected_orders++;
        break;
      default:
        break;
    }
  }
  
  std::cout << "[OMS] Order " << cl_ord_id << " state: " 
            << to_string(old_state) << " -> " << to_string(new_state) << std::endl;
  
  emit_order_event(event);
  emit_state_change(order_info);
}

void OrderManager::emit_order_event(const OrderEvent& event) {
  if (event_callback_) {
    event_callback_(event);
  }
  
  // TODO: Publish to ZeroMQ
  // event_publisher_.publish("order_events", serialize_event(event));
}

void OrderManager::emit_state_change(const OrderStateInfo& order_info) {
  if (state_callback_) {
    state_callback_(order_info);
  }
}
