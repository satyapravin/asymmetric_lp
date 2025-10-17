#pragma once
#include "enhanced_exchange_oms.hpp"
#include "exchange_monitor.hpp"
#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

// Enhanced mock exchange OMS with monitoring and rich error handling
class EnhancedMockExchangeOMS : public IEnhancedExchangeOMS {
public:
  EnhancedMockExchangeOMS(const std::string& exchange_name, 
                         double fill_probability = 0.8,
                         double reject_probability = 0.1,
                         std::chrono::milliseconds response_delay = std::chrono::milliseconds(100),
                         std::shared_ptr<IExchangeMonitor> monitor = nullptr);
  ~EnhancedMockExchangeOMS();

  // IEnhancedExchangeOMS interface
  Result<OrderResponse> send_order(const Order& order) override;
  Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override;
  Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                           double new_price, double new_qty) override;
  
  Result<bool> connect() override;
  void disconnect() override;
  bool is_connected() const override;
  
  std::string get_exchange_name() const override { return exchange_name_; }
  std::vector<std::string> get_supported_symbols() const override;
  
  Result<std::map<std::string, std::string>> get_health_status() const override;
  Result<std::map<std::string, double>> get_performance_metrics() const override;

private:
  void process_order(const Order& order);
  void simulate_ack(const Order& order, const std::string& exchange_order_id);
  void simulate_fill(const Order& order, const std::string& exchange_order_id);
  void simulate_reject(const Order& order, const std::string& reason);
  std::string generate_exchange_order_id();
  
  std::string exchange_name_;
  double fill_probability_;
  double reject_probability_;
  std::chrono::milliseconds response_delay_;
  
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};
  
  std::random_device rd_;
  std::mt19937 rng_;
  std::uniform_real_distribution<double> prob_dist_;
  
  mutable std::mutex orders_mutex_;
  std::map<std::string, std::string> cl_ord_to_exch_ord_;  // cl_ord_id -> exchange_order_id
  std::map<std::string, Order> active_orders_;             // exchange_order_id -> Order
  
  std::shared_ptr<IExchangeMonitor> monitor_;
  
  // Performance tracking
  std::atomic<uint64_t> total_orders_{0};
  std::atomic<uint64_t> successful_orders_{0};
  std::atomic<uint64_t> failed_orders_{0};
  std::atomic<double> total_latency_us_{0};
  std::atomic<uint64_t> latency_samples_{0};
};
