#pragma once
#include <string>
#include <chrono>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>
#include <functional>

// Performance metrics for exchange operations
struct ExchangeMetrics {
  std::atomic<uint64_t> total_orders{0};
  std::atomic<uint64_t> successful_orders{0};
  std::atomic<uint64_t> failed_orders{0};
  std::atomic<uint64_t> cancelled_orders{0};
  std::atomic<uint64_t> rejected_orders{0};
  std::atomic<uint64_t> filled_orders{0};
  
  std::atomic<double> total_volume{0.0};
  std::atomic<double> filled_volume{0.0};
  
  std::atomic<uint64_t> total_latency_us{0};
  std::atomic<uint64_t> latency_samples{0};
  
  std::atomic<uint64_t> connection_attempts{0};
  std::atomic<uint64_t> connection_failures{0};
  std::atomic<uint64_t> disconnections{0};
  
  std::chrono::system_clock::time_point start_time;
  
  ExchangeMetrics() : start_time(std::chrono::system_clock::now()) {}
  
  // Calculate average latency
  double get_avg_latency_us() const {
    uint64_t samples = latency_samples.load();
    return samples > 0 ? static_cast<double>(total_latency_us.load()) / samples : 0.0;
  }
  
  // Calculate success rate
  double get_success_rate() const {
    uint64_t total = total_orders.load();
    return total > 0 ? static_cast<double>(successful_orders.load()) / total : 0.0;
  }
  
  // Create a copyable version of the metrics
  struct CopyableMetrics {
    uint64_t total_orders;
    uint64_t successful_orders;
    uint64_t failed_orders;
    uint64_t cancelled_orders;
    uint64_t rejected_orders;
    uint64_t filled_orders;
    double total_volume;
    double filled_volume;
    uint64_t total_latency_us;
    uint64_t latency_samples;
    uint64_t connection_attempts;
    uint64_t connection_failures;
    uint64_t disconnections;
    std::chrono::system_clock::time_point start_time;
    
    CopyableMetrics() = default;
    
    CopyableMetrics(const ExchangeMetrics& metrics) 
      : total_orders(metrics.total_orders.load()),
        successful_orders(metrics.successful_orders.load()),
        failed_orders(metrics.failed_orders.load()),
        cancelled_orders(metrics.cancelled_orders.load()),
        rejected_orders(metrics.rejected_orders.load()),
        filled_orders(metrics.filled_orders.load()),
        total_volume(metrics.total_volume.load()),
        filled_volume(metrics.filled_volume.load()),
        total_latency_us(metrics.total_latency_us.load()),
        latency_samples(metrics.latency_samples.load()),
        connection_attempts(metrics.connection_attempts.load()),
        connection_failures(metrics.connection_failures.load()),
        disconnections(metrics.disconnections.load()),
        start_time(metrics.start_time) {}
    
    // Calculate average latency
    double get_avg_latency_us() const {
      return latency_samples > 0 ? static_cast<double>(total_latency_us) / latency_samples : 0.0;
    }
    
    // Calculate success rate
    double get_success_rate() const {
      return total_orders > 0 ? static_cast<double>(successful_orders) / total_orders : 0.0;
    }
    
    // Calculate fill rate
    double get_fill_rate() const {
      return total_orders > 0 ? static_cast<double>(filled_orders) / total_orders : 0.0;
    }
    
    // Calculate uptime in seconds
    double get_uptime_seconds() const {
      auto now = std::chrono::system_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
      return duration.count();
    }
  };
  
  // Calculate fill rate
  double get_fill_rate() const {
    uint64_t total = total_orders.load();
    return total > 0 ? static_cast<double>(filled_orders.load()) / total : 0.0;
  }
  
  // Get uptime in seconds
  double get_uptime_seconds() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    return duration.count();
  }
};

// Health status for exchanges
enum class HealthStatus {
  HEALTHY,
  DEGRADED,
  UNHEALTHY,
  UNKNOWN
};

struct HealthInfo {
  HealthStatus status;
  std::string message;
  std::chrono::system_clock::time_point last_check;
  std::map<std::string, std::string> details;
  
  HealthInfo() : status(HealthStatus::UNKNOWN), message("No data"), last_check(std::chrono::system_clock::now()) {}
  HealthInfo(HealthStatus s, const std::string& msg) 
    : status(s), message(msg), last_check(std::chrono::system_clock::now()) {}
};

// Monitoring and observability interface
class IExchangeMonitor {
public:
  virtual ~IExchangeMonitor() = default;
  
  // Metrics collection
  virtual void record_order_attempt(const std::string& exchange, const std::string& symbol) = 0;
  virtual void record_order_success(const std::string& exchange, const std::string& symbol, 
                                   double volume, uint64_t latency_us) = 0;
  virtual void record_order_failure(const std::string& exchange, const std::string& symbol, 
                                   const std::string& error_code) = 0;
  virtual void record_order_fill(const std::string& exchange, const std::string& symbol, 
                                double volume) = 0;
  virtual void record_connection_event(const std::string& exchange, bool success) = 0;
  
  // Metrics retrieval
  virtual ExchangeMetrics::CopyableMetrics get_metrics(const std::string& exchange) const = 0;
  virtual std::map<std::string, ExchangeMetrics::CopyableMetrics> get_all_metrics() const = 0;
  
  // Health monitoring
  virtual HealthInfo get_health_status(const std::string& exchange) const = 0;
  virtual std::map<std::string, HealthInfo> get_all_health_status() const = 0;
  
  // Callbacks for alerts
  virtual void set_health_alert_callback(std::function<void(const std::string&, HealthStatus)> callback) = 0;
  virtual void set_performance_alert_callback(std::function<void(const std::string&, const std::string&)> callback) = 0;
  
  // Reporting
  virtual void print_metrics_summary() const = 0;
  virtual void print_health_summary() const = 0;
};

// Concrete implementation of exchange monitor
class ExchangeMonitor : public IExchangeMonitor {
public:
  ExchangeMonitor();
  ~ExchangeMonitor() = default;
  
  // Metrics collection
  void record_order_attempt(const std::string& exchange, const std::string& symbol) override;
  void record_order_success(const std::string& exchange, const std::string& symbol, 
                           double volume, uint64_t latency_us) override;
  void record_order_failure(const std::string& exchange, const std::string& symbol, 
                           const std::string& error_code) override;
  void record_order_fill(const std::string& exchange, const std::string& symbol, 
                        double volume) override;
  void record_connection_event(const std::string& exchange, bool success) override;
  
  // Metrics retrieval
  ExchangeMetrics::CopyableMetrics get_metrics(const std::string& exchange) const override;
  std::map<std::string, ExchangeMetrics::CopyableMetrics> get_all_metrics() const override;
  
  // Health monitoring
  HealthInfo get_health_status(const std::string& exchange) const override;
  std::map<std::string, HealthInfo> get_all_health_status() const override;
  
  // Callbacks
  void set_health_alert_callback(std::function<void(const std::string&, HealthStatus)> callback) override;
  void set_performance_alert_callback(std::function<void(const std::string&, const std::string&)> callback) override;
  
  // Reporting
  void print_metrics_summary() const override;
  void print_health_summary() const override;
  
private:
  mutable std::mutex metrics_mutex_;
  std::map<std::string, ExchangeMetrics> exchange_metrics_;
  
  mutable std::mutex health_mutex_;
  std::map<std::string, HealthInfo> exchange_health_;
  
  std::function<void(const std::string&, HealthStatus)> health_alert_callback_;
  std::function<void(const std::string&, const std::string&)> performance_alert_callback_;
  
  void update_health_status(const std::string& exchange);
  void check_performance_thresholds(const std::string& exchange);
};
