#pragma once
#include "enhanced_oms.hpp"
#include <random>
#include <thread>
#include <chrono>

// Mock exchange OMS for testing
class MockExchangeOMS : public IExchangeOMS {
public:
  MockExchangeOMS(const std::string& exchange_name, 
                  double fill_probability = 0.8,
                  double reject_probability = 0.1,
                  std::chrono::milliseconds response_delay = std::chrono::milliseconds(100));
  ~MockExchangeOMS();

  // IExchangeOMS interface
  bool send_order(const Order& order) override;
  bool cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override;
  bool modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                   double new_price, double new_qty) override;
  
  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  
  std::string get_exchange_name() const override { return exchange_name_; }
  std::vector<std::string> get_supported_symbols() const override;

private:
  void process_order(const Order& order);
  void simulate_fill(const Order& order, const std::string& exchange_order_id);
  void simulate_reject(const Order& order, const std::string& reason);
  void simulate_ack(const Order& order, const std::string& exchange_order_id);
  
  std::string generate_exchange_order_id();
  
  std::string exchange_name_;
  double fill_probability_;
  double reject_probability_;
  std::chrono::milliseconds response_delay_;
  
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};
  
  std::mt19937 rng_;
  std::uniform_real_distribution<double> prob_dist_;
  
  // Order tracking
  std::mutex orders_mutex_;
  std::unordered_map<std::string, std::string> cl_ord_to_exch_ord_; // cl_ord_id -> exchange_order_id
  std::unordered_map<std::string, Order> active_orders_; // exchange_order_id -> Order
};
