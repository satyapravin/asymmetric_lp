#pragma once
#include "i_oms.hpp"
#include "i_exchange_handler.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <vector>

// Multi-exchange Order Management System
class MultiExchangeOMS : public IOMS {
public:
  MultiExchangeOMS();
  ~MultiExchangeOMS() override;
  
  // IOMS interface
  bool start() override;
  void stop() override;
  
  bool send_order(const std::string& client_order_id,
                 const std::string& exchange,
                 const std::string& symbol,
                 uint32_t side,
                 uint32_t type,
                 double quantity,
                 double price) override;
  
  bool cancel_order(const std::string& client_order_id, const std::string& exchange) override;
  bool modify_order(const std::string& client_order_id,
                   const std::string& exchange,
                   const std::string& symbol,
                   uint32_t side,
                   uint32_t type,
                   double quantity,
                   double price) override;
  
  std::vector<Order> get_open_orders(const std::string& exchange = "") override;
  Order get_order_status(const std::string& client_order_id, const std::string& exchange) override;
  
  void set_order_event_callback(OrderEventCallback callback) override { order_event_callback_ = callback; }
  
  // Exchange management
  void add_exchange(const std::string& exchange_name, std::unique_ptr<IExchangeHandler> handler) override;
  void remove_exchange(const std::string& exchange_name) override;
  std::vector<std::string> get_available_exchanges() override;
  
private:
  // Order tracking
  struct OrderInfo {
    std::string client_order_id;
    std::string exchange;
    std::string symbol;
    uint32_t side;
    uint32_t type;
    double quantity;
    double price;
    OrderStatus status;
    uint64_t timestamp_us;
  };
  
  // Internal methods
  IExchangeHandler* get_exchange_handler(const std::string& exchange);
  void handle_exchange_order_event(const std::string& exchange, const Order& order);
  Order convert_to_order(const OrderInfo& info);
  
  // Exchange handlers
  std::map<std::string, std::unique_ptr<IExchangeHandler>> exchange_handlers_;
  std::mutex handlers_mutex_;
  
  // Order tracking
  std::map<std::string, OrderInfo> order_tracking_;
  std::mutex orders_mutex_;
  
  // State
  std::atomic<bool> running_{false};
  
  // Callbacks
  OrderEventCallback order_event_callback_;
};
