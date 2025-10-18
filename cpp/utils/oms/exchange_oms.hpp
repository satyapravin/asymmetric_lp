#pragma once
#include <string>
#include <exception>
#include <chrono>
#include <map>
#include <optional>
#include <sstream>
#include "order.hpp"
#include "order_state.hpp"
#include "oms.hpp"  // For IExchangeOMS interface

// Rich error information for exchange operations
struct ExchangeError {
  std::string error_code;
  std::string error_message;
  std::string exchange_name;
  std::string operation;
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> context;
  
  ExchangeError(const std::string& code, const std::string& message, 
                const std::string& exchange, const std::string& op)
    : error_code(code), error_message(message), exchange_name(exchange), 
      operation(op), timestamp(std::chrono::system_clock::now()) {}
  
  std::string to_string() const {
    std::ostringstream oss;
    oss << "[" << exchange_name << "] " << operation << " failed: " 
        << error_code << " - " << error_message;
    return oss.str();
  }
};

// Result wrapper for operations that can fail
template<typename T>
class Result {
public:
  // Success constructor
  Result(T&& value) : value_(std::move(value)), success_(true) {}
  
  // Error constructor
  Result(ExchangeError error) : error_(std::move(error)), success_(false) {}
  
  // Check if operation was successful
  bool is_success() const { return success_; }
  bool is_error() const { return !success_; }
  
  // Get the value (only call if is_success() == true)
  const T& value() const { return value_.value(); }
  T& value() { return value_.value(); }
  
  // Get the error (only call if is_error() == true)
  const ExchangeError& error() const { return error_.value(); }
  
  // Convenience operators
  explicit operator bool() const { return success_; }
  const T& operator*() const { return value(); }
  T& operator*() { return value(); }
  
private:
  std::optional<T> value_;
  std::optional<ExchangeError> error_;
  bool success_;
};

// Order response with rich information
struct OrderResponse {
  std::string cl_ord_id;
  std::string exchange_order_id;
  std::string exchange_name;
  std::string symbol;
  double qty;
  double price;
  std::string side;
  std::string status;  // "PENDING", "FILLED", "CANCELLED", "REJECTED"
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> metadata;
  
  OrderResponse(const std::string& cl_id, const std::string& exch_id, 
                const std::string& exchange, const std::string& sym)
    : cl_ord_id(cl_id), exchange_order_id(exch_id), exchange_name(exchange),
      symbol(sym), timestamp(std::chrono::system_clock::now()) {}
};

// Enhanced exchange OMS interface with rich error handling
class IEnhancedExchangeOMS : public IExchangeOMS {
public:
  virtual ~IEnhancedExchangeOMS() = default;
  
  // Enhanced order operations with rich error handling
  virtual Result<OrderResponse> send_order(const Order& order) = 0;
  virtual Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) = 0;
  virtual Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                                   double new_price, double new_qty) = 0;
  
  // Connection management
  virtual Result<bool> connect() = 0;
  virtual void disconnect() = 0;
  virtual bool is_connected() const = 0;
  
  // Exchange information
  virtual std::string get_exchange_name() const = 0;
  virtual std::vector<std::string> get_supported_symbols() const = 0;
  
  // Health and monitoring
  virtual Result<std::map<std::string, std::string>> get_health_status() const = 0;
  virtual Result<std::map<std::string, double>> get_performance_metrics() const = 0;
  
  // Event handling
  std::function<void(const OrderEvent&)> on_order_event;
  std::function<void(const ExchangeError&)> on_error;
  
  // Implement base IExchangeOMS interface
  void send(const Order& order) override {
    auto result = send_order(order);
    if (result.is_error() && on_error) {
      on_error(result.error());
    }
  }
  
  void cancel(const std::string& cl_ord_id) override {
    auto result = cancel_order(cl_ord_id, "");
    if (result.is_error() && on_error) {
      on_error(result.error());
    }
  }
};
