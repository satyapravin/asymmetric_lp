#include "enhanced_oms.hpp"
#include <iostream>
#include <algorithm>

OMS::OMS() {
}

OMS::~OMS() {
  disconnect_all_exchanges();
}

void OMS::register_exchange(const std::string& exch, std::shared_ptr<IExchangeOMS> handler) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  handlers_[exch] = std::move(handler);
  
  // Set up event callback to fan-in to OMS callback
  handlers_[exch]->on_order_event = [this, exch](const OrderEvent& event) {
    handle_exchange_event(exch, event);
  };
  
  std::cout << "[OMS] Registered exchange: " << exch << std::endl;
}

void OMS::unregister_exchange(const std::string& exch) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = handlers_.find(exch);
  if (it != handlers_.end()) {
    it->second->disconnect();
    handlers_.erase(it);
    std::cout << "[OMS] Unregistered exchange: " << exch << std::endl;
  }
}

bool OMS::send_order(const Order& order) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = handlers_.find(order.exch);
  if (it == handlers_.end()) {
    std::cout << "[OMS] Unknown exchange: " << order.exch << std::endl;
    
    // Send reject event
    if (event_callback_) {
      OrderEvent reject_event;
      reject_event.cl_ord_id = order.cl_ord_id;
      reject_event.exch = order.exch;
      reject_event.symbol = order.symbol;
      reject_event.type = OrderEventType::Reject;
      reject_event.text = "Unknown exchange: " + order.exch;
      reject_event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
      
      event_callback_(reject_event);
    }
    return false;
  }
  
  bool success = it->second->send_order(order);
  if (!success) {
    std::cout << "[OMS] Failed to send order to " << order.exch << std::endl;
  }
  
  return success;
}

bool OMS::cancel_order(const std::string& exch, const std::string& cl_ord_id, 
                      const std::string& exchange_order_id) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = handlers_.find(exch);
  if (it == handlers_.end()) {
    std::cout << "[OMS] Unknown exchange: " << exch << std::endl;
    return false;
  }
  
  bool success = it->second->cancel_order(cl_ord_id, exchange_order_id);
  if (!success) {
    std::cout << "[OMS] Failed to cancel order on " << exch << std::endl;
  }
  
  return success;
}

bool OMS::modify_order(const std::string& exch, const std::string& cl_ord_id,
                      const std::string& exchange_order_id, double new_price, double new_qty) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  auto it = handlers_.find(exch);
  if (it == handlers_.end()) {
    std::cout << "[OMS] Unknown exchange: " << exch << std::endl;
    return false;
  }
  
  bool success = it->second->modify_order(cl_ord_id, exchange_order_id, new_price, new_qty);
  if (!success) {
    std::cout << "[OMS] Failed to modify order on " << exch << std::endl;
  }
  
  return success;
}

bool OMS::connect_all_exchanges() {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  bool all_connected = true;
  for (const auto& pair : handlers_) {
    const std::string& exchange_name = pair.first;
    const auto& handler = pair.second;
    
    std::cout << "[OMS] Connecting to " << exchange_name << "..." << std::endl;
    bool connected = handler->connect();
    
    if (connected) {
      std::cout << "[OMS] Connected to " << exchange_name << std::endl;
    } else {
      std::cout << "[OMS] Failed to connect to " << exchange_name << std::endl;
      all_connected = false;
    }
  }
  
  return all_connected;
}

void OMS::disconnect_all_exchanges() {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  for (const auto& pair : handlers_) {
    const std::string& exchange_name = pair.first;
    const auto& handler = pair.second;
    
    std::cout << "[OMS] Disconnecting from " << exchange_name << "..." << std::endl;
    handler->disconnect();
    std::cout << "[OMS] Disconnected from " << exchange_name << std::endl;
  }
}

void OMS::handle_exchange_event(const std::string& exchange_name, const OrderEvent& event) {
  std::cout << "[OMS] Received event from " << exchange_name << ": " 
            << event.cl_ord_id << " " << to_string(event.type) << std::endl;
  
  if (event_callback_) {
    event_callback_(event);
  }
}

std::vector<std::string> OMS::get_registered_exchanges() const {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  
  std::vector<std::string> exchanges;
  for (const auto& pair : handlers_) {
    exchanges.push_back(pair.first);
  }
  
  return exchanges;
}

bool OMS::is_exchange_registered(const std::string& exch) const {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  return handlers_.find(exch) != handlers_.end();
}
