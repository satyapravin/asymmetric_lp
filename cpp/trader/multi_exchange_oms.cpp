#include "multi_exchange_oms.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

MultiExchangeOMS::MultiExchangeOMS() = default;

MultiExchangeOMS::~MultiExchangeOMS() {
  stop();
}

bool MultiExchangeOMS::start() {
  if (running_.load()) return true;
  
  running_.store(true);
  
  // Start all exchange handlers
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  for (auto& [name, handler] : exchange_handlers_) {
    if (!handler->start()) {
      std::cerr << "[OMS] Failed to start exchange handler: " << name << std::endl;
      return false;
    }
  }
  
  std::cout << "[OMS] Started with " << exchange_handlers_.size() << " exchanges" << std::endl;
  return true;
}

void MultiExchangeOMS::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  // Stop all exchange handlers
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  for (auto& [name, handler] : exchange_handlers_) {
    handler->stop();
  }
  
  std::cout << "[OMS] Stopped" << std::endl;
}

bool MultiExchangeOMS::send_order(const std::string& client_order_id,
                                 const std::string& exchange,
                                 const std::string& symbol,
                                 uint32_t side,
                                 uint32_t type,
                                 double quantity,
                                 double price) {
  IExchangeHandler* handler = get_exchange_handler(exchange);
  if (!handler) {
    std::cerr << "[OMS] No handler for exchange: " << exchange << std::endl;
    return false;
  }
  
  // Create order
  Order order;
  order.client_order_id = client_order_id;
  order.symbol = symbol;
  order.side = static_cast<OrderSide>(side);
  order.type = static_cast<OrderType>(type);
  order.quantity = quantity;
  order.price = price;
  order.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  // Track order
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    OrderInfo info;
    info.client_order_id = client_order_id;
    info.exchange = exchange;
    info.symbol = symbol;
    info.side = side;
    info.type = type;
    info.quantity = quantity;
    info.price = price;
    info.status = OrderStatus::PENDING;
    info.timestamp_us = order.timestamp_us;
    
    order_tracking_[client_order_id] = info;
  }
  
  // Send to exchange
  bool success = handler->send_order(order);
  
  if (success) {
    std::cout << "[OMS] Sent order " << client_order_id << " to " << exchange 
              << " " << symbol << " " << quantity << " @ " << price << std::endl;
  } else {
    std::cerr << "[OMS] Failed to send order " << client_order_id << " to " << exchange << std::endl;
  }
  
  return success;
}

bool MultiExchangeOMS::cancel_order(const std::string& client_order_id, const std::string& exchange) {
  IExchangeHandler* handler = get_exchange_handler(exchange);
  if (!handler) {
    std::cerr << "[OMS] No handler for exchange: " << exchange << std::endl;
    return false;
  }
  
  bool success = handler->cancel_order(client_order_id);
  
  if (success) {
    // Update order status
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = order_tracking_.find(client_order_id);
    if (it != order_tracking_.end()) {
      it->second.status = OrderStatus::CANCELLED;
    }
    
    std::cout << "[OMS] Cancelled order " << client_order_id << " on " << exchange << std::endl;
  } else {
    std::cerr << "[OMS] Failed to cancel order " << client_order_id << " on " << exchange << std::endl;
  }
  
  return success;
}

bool MultiExchangeOMS::modify_order(const std::string& client_order_id,
                                   const std::string& exchange,
                                   const std::string& symbol,
                                   uint32_t side,
                                   uint32_t type,
                                   double quantity,
                                   double price) {
  IExchangeHandler* handler = get_exchange_handler(exchange);
  if (!handler) {
    std::cerr << "[OMS] No handler for exchange: " << exchange << std::endl;
    return false;
  }
  
  // Create modified order
  Order order;
  order.client_order_id = client_order_id;
  order.symbol = symbol;
  order.side = static_cast<OrderSide>(side);
  order.type = static_cast<OrderType>(type);
  order.quantity = quantity;
  order.price = price;
  
  bool success = handler->modify_order(order);
  
  if (success) {
    // Update order tracking
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = order_tracking_.find(client_order_id);
    if (it != order_tracking_.end()) {
      it->second.symbol = symbol;
      it->second.side = side;
      it->second.type = type;
      it->second.quantity = quantity;
      it->second.price = price;
    }
    
    std::cout << "[OMS] Modified order " << client_order_id << " on " << exchange << std::endl;
  } else {
    std::cerr << "[OMS] Failed to modify order " << client_order_id << " on " << exchange << std::endl;
  }
  
  return success;
}

std::vector<Order> MultiExchangeOMS::get_open_orders(const std::string& exchange) {
  std::vector<Order> open_orders;
  
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  if (exchange.empty()) {
    // Get orders from all exchanges
    for (auto& [name, handler] : exchange_handlers_) {
      auto exchange_orders = handler->get_open_orders();
      open_orders.insert(open_orders.end(), exchange_orders.begin(), exchange_orders.end());
    }
  } else {
    // Get orders from specific exchange
    auto it = exchange_handlers_.find(exchange);
    if (it != exchange_handlers_.end()) {
      open_orders = it->second->get_open_orders();
    }
  }
  
  return open_orders;
}

Order MultiExchangeOMS::get_order_status(const std::string& client_order_id, const std::string& exchange) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = order_tracking_.find(client_order_id);
  if (it != order_tracking_.end()) {
    return convert_to_order(it->second);
  }
  
  return Order{}; // Return empty order if not found
}

void MultiExchangeOMS::add_exchange(const std::string& exchange_name, std::unique_ptr<IExchangeHandler> handler) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  // Set up order event callback
  handler->set_order_event_callback([this, exchange_name](const Order& order) {
    this->handle_exchange_order_event(exchange_name, order);
  });
  
  exchange_handlers_[exchange_name] = std::move(handler);
  std::cout << "[OMS] Added exchange: " << exchange_name << std::endl;
}

void MultiExchangeOMS::remove_exchange(const std::string& exchange_name) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = exchange_handlers_.find(exchange_name);
  if (it != exchange_handlers_.end()) {
    it->second->stop();
    exchange_handlers_.erase(it);
    std::cout << "[OMS] Removed exchange: " << exchange_name << std::endl;
  }
}

std::vector<std::string> MultiExchangeOMS::get_available_exchanges() {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  std::vector<std::string> exchanges;
  for (const auto& [name, handler] : exchange_handlers_) {
    exchanges.push_back(name);
  }
  
  return exchanges;
}

IExchangeHandler* MultiExchangeOMS::get_exchange_handler(const std::string& exchange) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = exchange_handlers_.find(exchange);
  return (it != exchange_handlers_.end()) ? it->second.get() : nullptr;
}

void MultiExchangeOMS::handle_exchange_order_event(const std::string& exchange, const Order& order) {
  // Update order tracking
  {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = order_tracking_.find(order.client_order_id);
    if (it != order_tracking_.end()) {
      it->second.status = order.status;
      it->second.filled_quantity = order.filled_quantity;
      it->second.average_price = order.average_price;
    }
  }
  
  // Forward to OMS callback
  if (order_event_callback_) {
    uint32_t event_type = static_cast<uint32_t>(order.status);
    order_event_callback_(order.client_order_id, exchange, order.symbol, 
                         event_type, order.filled_quantity, order.average_price, order.error_message);
  }
}

Order MultiExchangeOMS::convert_to_order(const OrderInfo& info) {
  Order order;
  order.client_order_id = info.client_order_id;
  order.symbol = info.symbol;
  order.side = static_cast<OrderSide>(info.side);
  order.type = static_cast<OrderType>(info.type);
  order.quantity = info.quantity;
  order.price = info.price;
  order.status = info.status;
  order.timestamp_us = info.timestamp_us;
  
  return order;
}
