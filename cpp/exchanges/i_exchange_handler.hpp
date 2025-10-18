#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>

// Order types
enum class OrderSide { BUY = 0, SELL = 1 };
enum class OrderType { MARKET = 0, LIMIT = 1, STOP = 2 };
enum class OrderStatus { PENDING = 0, FILLED = 1, CANCELLED = 2, REJECTED = 3 };

// Order structure
struct Order {
  std::string client_order_id;
  std::string exchange_order_id;
  std::string symbol;
  OrderSide side;
  OrderType type;
  double quantity;
  double price;
  double filled_quantity{0.0};
  double average_price{0.0};
  OrderStatus status{OrderStatus::PENDING};
  uint64_t timestamp_us{0};
  std::string error_message;
};

// Order event callback
using OrderEventCallback = std::function<void(const Order& order)>;

// Base interface for exchange-specific order handlers
class IExchangeHandler {
public:
  virtual ~IExchangeHandler() = default;
  
  // Lifecycle
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual bool is_connected() const = 0;
  
  // Order management
  virtual bool send_order(const Order& order) = 0;
  virtual bool cancel_order(const std::string& client_order_id) = 0;
  virtual bool modify_order(const Order& order) = 0;
  
  // Order queries
  virtual std::vector<Order> get_open_orders() = 0;
  virtual Order get_order_status(const std::string& client_order_id) = 0;
  
  // Callbacks
  virtual void set_order_event_callback(OrderEventCallback callback) = 0;
  
  // Exchange-specific configuration
  virtual void set_api_key(const std::string& key) {}
  virtual void set_secret_key(const std::string& secret) {}
  virtual void set_passphrase(const std::string& passphrase) {}
  virtual void set_sandbox_mode(bool enabled) {}
  
  // Exchange-specific methods
  virtual std::string get_exchange_name() const = 0;
};
