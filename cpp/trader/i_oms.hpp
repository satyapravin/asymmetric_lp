#pragma once
#include "i_exchange_handler.hpp"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>

// Order Management System interface
class IOMS {
public:
  using OrderEventCallback = std::function<void(const std::string& client_order_id,
                                               const std::string& exchange,
                                               const std::string& symbol,
                                               uint32_t event_type,
                                               double fill_qty,
                                               double fill_price,
                                               const std::string& text)>;
  
  virtual ~IOMS() = default;
  
  // Lifecycle
  virtual bool start() = 0;
  virtual void stop() = 0;
  
  // Order management
  virtual bool send_order(const std::string& client_order_id,
                         const std::string& exchange,
                         const std::string& symbol,
                         uint32_t side,
                         uint32_t type,
                         double quantity,
                         double price) = 0;
  
  virtual bool cancel_order(const std::string& client_order_id, const std::string& exchange) = 0;
  virtual bool modify_order(const std::string& client_order_id,
                           const std::string& exchange,
                           const std::string& symbol,
                           uint32_t side,
                           uint32_t type,
                           double quantity,
                           double price) = 0;
  
  // Order queries
  virtual std::vector<Order> get_open_orders(const std::string& exchange = "") = 0;
  virtual Order get_order_status(const std::string& client_order_id, const std::string& exchange) = 0;
  
  // Callbacks
  virtual void set_order_event_callback(OrderEventCallback callback) = 0;
  
  // Exchange management
  virtual void add_exchange(const std::string& exchange_name, std::unique_ptr<IExchangeHandler> handler) = 0;
  virtual void remove_exchange(const std::string& exchange_name) = 0;
  virtual std::vector<std::string> get_available_exchanges() = 0;
};

// OMS factory
class OMSFactory {
public:
  static std::unique_ptr<IOMS> create();
};
