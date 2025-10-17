#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <queue>
#include <condition_variable>
#include "order.hpp"
#include "order_state.hpp"
#include "../zmq/zmq_publisher.hpp"
#include "../zmq/zmq_subscriber.hpp"

// Enhanced OMS with state management and persistence
class OrderManager {
public:
  using EventCallback = std::function<void(const OrderEvent&)>;
  using OrderStateCallback = std::function<void(const OrderStateInfo&)>;

  OrderManager(const std::string& request_endpoint, 
               const std::string& event_endpoint);
  ~OrderManager();

  // Order lifecycle management
  bool submit_order(const Order& order);
  bool cancel_order(const std::string& cl_ord_id);
  bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty);
  
  // State queries
  OrderStateInfo get_order_state(const std::string& cl_ord_id) const;
  std::vector<OrderStateInfo> get_active_orders() const;
  std::vector<OrderStateInfo> get_orders_by_symbol(const std::string& symbol) const;
  std::vector<OrderStateInfo> get_orders_by_exchange(const std::string& exch) const;
  
  // Statistics
  struct OrderStats {
    uint64_t total_orders{0};
    uint64_t filled_orders{0};
    uint64_t cancelled_orders{0};
    uint64_t rejected_orders{0};
    double total_volume{0.0};
    double total_filled_volume{0.0};
  };
  OrderStats get_statistics() const;
  
  // Event callbacks
  void set_event_callback(EventCallback callback) { event_callback_ = std::move(callback); }
  void set_state_callback(OrderStateCallback callback) { state_callback_ = std::move(callback); }
  
  // Lifecycle management
  void start();
  void stop();
  bool is_running() const { return running_.load(); }

private:
  // Order processing
  void process_order_requests();
  void process_order_events();
  void cleanup_expired_orders();
  
  // State management
  void update_order_state(const std::string& cl_ord_id, OrderState new_state, 
                         const OrderEvent& event);
  void emit_order_event(const OrderEvent& event);
  void emit_state_change(const OrderStateInfo& order_info);
  
  // Order storage
  mutable std::mutex orders_mutex_;
  std::unordered_map<std::string, OrderStateInfo> orders_;
  
  // ZeroMQ communication
  ZmqSubscriber request_subscriber_;
  ZmqPublisher event_publisher_;
  
  // Callbacks
  EventCallback event_callback_;
  OrderStateCallback state_callback_;
  
  // Threading
  std::atomic<bool> running_{false};
  std::thread request_thread_;
  std::thread event_thread_;
  std::thread cleanup_thread_;
  
  // Configuration
  std::chrono::seconds order_timeout_{300}; // 5 minutes default timeout
  std::chrono::seconds cleanup_interval_{60}; // 1 minute cleanup interval
  
  // Statistics
  mutable std::mutex stats_mutex_;
  OrderStats stats_;
};

// Exchange-specific OMS interface (enhanced)
class IExchangeOMS {
public:
  virtual ~IExchangeOMS() = default;
  
  // Order operations
  virtual bool send_order(const Order& order) = 0;
  virtual bool cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) = 0;
  virtual bool modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                           double new_price, double new_qty) = 0;
  
  // Connection management
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool is_connected() const = 0;
  
  // Event handling
  std::function<void(const OrderEvent&)> on_order_event;
  
  // Exchange info
  virtual std::string get_exchange_name() const = 0;
  virtual std::vector<std::string> get_supported_symbols() const = 0;
};

// Router OMS: routes by order.exch to registered handlers
class OMS {
public:
  using EventCallback = std::function<void(const OrderEvent&)>;

  OMS();
  ~OMS();

  // Exchange registration
  void register_exchange(const std::string& exch, std::shared_ptr<IExchangeOMS> handler);
  void unregister_exchange(const std::string& exch);
  
  // Order operations
  bool send_order(const Order& order);
  bool cancel_order(const std::string& exch, const std::string& cl_ord_id, 
                   const std::string& exchange_order_id = "");
  bool modify_order(const std::string& exch, const std::string& cl_ord_id,
                   const std::string& exchange_order_id, double new_price, double new_qty);
  
  // Connection management
  bool connect_all_exchanges();
  void disconnect_all_exchanges();
  
  // Event handling
  void set_event_callback(EventCallback callback) { event_callback_ = std::move(callback); }
  
  // Exchange info
  std::vector<std::string> get_registered_exchanges() const;
  bool is_exchange_registered(const std::string& exch) const;

private:
  void handle_exchange_event(const std::string& exchange_name, const OrderEvent& event);
  
  mutable std::mutex handlers_mutex_;
  std::unordered_map<std::string, std::shared_ptr<IExchangeOMS>> handlers_;
  
  EventCallback event_callback_;
};
