#include "exchange_monitor.hpp"
#include "../logging/log_helper.hpp"
#include <iomanip>
#include <sstream>

ExchangeMonitor::ExchangeMonitor() {
  // Set up default alert thresholds
  health_alert_callback_ = [](const std::string& exchange, HealthStatus status) {
    if (status == HealthStatus::UNHEALTHY) {
      LOG_ERROR_COMP("MONITOR", "ALERT: Exchange " + exchange + " is UNHEALTHY!");
    } else if (status == HealthStatus::DEGRADED) {
      LOG_WARN_COMP("MONITOR", "WARNING: Exchange " + exchange + " is DEGRADED");
    }
  };
  
  performance_alert_callback_ = [](const std::string& exchange, const std::string& message) {
    LOG_WARN_COMP("MONITOR", "PERFORMANCE ALERT: " + exchange + " - " + message);
  };
}

void ExchangeMonitor::record_order_attempt(const std::string& exchange, const std::string& symbol) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  exchange_metrics_[exchange].total_orders++;
}

void ExchangeMonitor::record_order_success(const std::string& exchange, const std::string& symbol, 
                                          double volume, uint64_t latency_us) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto& metrics = exchange_metrics_[exchange];
  metrics.successful_orders++;
  double current_volume = metrics.total_volume.load();
  metrics.total_volume.store(current_volume + volume);
  metrics.total_latency_us += latency_us;
  metrics.latency_samples++;
  
  check_performance_thresholds(exchange);
}

void ExchangeMonitor::record_order_failure(const std::string& exchange, const std::string& symbol, 
                                          const std::string& error_code) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  exchange_metrics_[exchange].failed_orders++;
  
  update_health_status(exchange);
}

void ExchangeMonitor::record_order_fill(const std::string& exchange, const std::string& symbol, 
                                       double volume) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto& metrics = exchange_metrics_[exchange];
  metrics.filled_orders++;
  double current_filled_volume = metrics.filled_volume.load();
  metrics.filled_volume.store(current_filled_volume + volume);
}

void ExchangeMonitor::record_connection_event(const std::string& exchange, bool success) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto& metrics = exchange_metrics_[exchange];
  
  if (success) {
    metrics.connection_attempts++;
  } else {
    metrics.connection_attempts++;
    metrics.connection_failures++;
  }
  
  update_health_status(exchange);
}

ExchangeMetrics::CopyableMetrics ExchangeMonitor::get_metrics(const std::string& exchange) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto it = exchange_metrics_.find(exchange);
  if (it != exchange_metrics_.end()) {
    return ExchangeMetrics::CopyableMetrics(it->second);
  }
  return ExchangeMetrics::CopyableMetrics(ExchangeMetrics{});
}

std::map<std::string, ExchangeMetrics::CopyableMetrics> ExchangeMonitor::get_all_metrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  std::map<std::string, ExchangeMetrics::CopyableMetrics> result;
  for (const auto& [exchange, metrics] : exchange_metrics_) {
    result[exchange] = ExchangeMetrics::CopyableMetrics(metrics);
  }
  return result;
}

HealthInfo ExchangeMonitor::get_health_status(const std::string& exchange) const {
  std::lock_guard<std::mutex> lock(health_mutex_);
  auto it = exchange_health_.find(exchange);
  if (it != exchange_health_.end()) {
    return it->second;
  }
  return HealthInfo(HealthStatus::UNKNOWN, "No health data available");
}

std::map<std::string, HealthInfo> ExchangeMonitor::get_all_health_status() const {
  std::lock_guard<std::mutex> lock(health_mutex_);
  return exchange_health_;
}

void ExchangeMonitor::set_health_alert_callback(std::function<void(const std::string&, HealthStatus)> callback) {
  health_alert_callback_ = callback;
}

void ExchangeMonitor::set_performance_alert_callback(std::function<void(const std::string&, const std::string&)> callback) {
  performance_alert_callback_ = callback;
}

void ExchangeMonitor::print_metrics_summary() const {
  LOG_INFO_COMP("MONITOR", "\n=== Exchange Performance Metrics ===");
  
  auto all_metrics = get_all_metrics();
  for (const auto& [exchange, metrics] : all_metrics) {
    std::stringstream ss;
    ss << "\n[" << exchange << "]\n"
       << "  Total Orders: " << metrics.total_orders << "\n"
       << "  Success Rate: " << std::fixed << std::setprecision(2) 
       << metrics.get_success_rate() * 100 << "%\n"
       << "  Fill Rate: " << std::fixed << std::setprecision(2) 
       << metrics.get_fill_rate() * 100 << "%\n"
       << "  Avg Latency: " << std::fixed << std::setprecision(2) 
       << metrics.get_avg_latency_us() / 1000.0 << " ms\n"
       << "  Total Volume: " << std::fixed << std::setprecision(2) 
       << metrics.total_volume << "\n"
       << "  Filled Volume: " << std::fixed << std::setprecision(2) 
       << metrics.filled_volume << "\n"
       << "  Uptime: " << std::fixed << std::setprecision(1) 
       << metrics.get_uptime_seconds() << " seconds";
    LOG_INFO_COMP("MONITOR", ss.str());
  }
}

void ExchangeMonitor::print_health_summary() const {
  LOG_INFO_COMP("MONITOR", "\n=== Exchange Health Status ===");
  
  auto all_health = get_all_health_status();
  for (const auto& [exchange, health] : all_health) {
    std::string status_str;
    switch (health.status) {
      case HealthStatus::HEALTHY: status_str = "HEALTHY"; break;
      case HealthStatus::DEGRADED: status_str = "DEGRADED"; break;
      case HealthStatus::UNHEALTHY: status_str = "UNHEALTHY"; break;
      case HealthStatus::UNKNOWN: status_str = "UNKNOWN"; break;
    }
    
    LOG_INFO_COMP("MONITOR", "[" + exchange + "] " + status_str + " - " + health.message);
  }
}

void ExchangeMonitor::update_health_status(const std::string& exchange) {
  std::lock_guard<std::mutex> lock(health_mutex_);
  
  auto metrics_it = exchange_metrics_.find(exchange);
  if (metrics_it == exchange_metrics_.end()) {
    exchange_health_[exchange] = HealthInfo(HealthStatus::UNKNOWN, "No metrics available");
    return;
  }
  
  const auto& metrics = metrics_it->second;
  
  // Calculate health based on metrics
  HealthStatus status = HealthStatus::HEALTHY;
  std::string message = "All systems operational";
  
  // Check success rate
  double success_rate = metrics.get_success_rate();
  if (success_rate < 0.5) {
    status = HealthStatus::UNHEALTHY;
    message = "Low success rate: " + std::to_string(success_rate * 100) + "%";
  } else if (success_rate < 0.8) {
    status = HealthStatus::DEGRADED;
    message = "Degraded success rate: " + std::to_string(success_rate * 100) + "%";
  }
  
  // Check connection failures
  uint64_t failures = metrics.connection_failures.load();
  uint64_t attempts = metrics.connection_attempts.load();
  if (attempts > 0) {
    double failure_rate = static_cast<double>(failures) / attempts;
    if (failure_rate > 0.3) {
      status = HealthStatus::UNHEALTHY;
      message = "High connection failure rate: " + std::to_string(failure_rate * 100) + "%";
    }
  }
  
  // Check latency
  double avg_latency_ms = metrics.get_avg_latency_us() / 1000.0;
  if (avg_latency_ms > 1000) {  // > 1 second
    if (status == HealthStatus::HEALTHY) {
      status = HealthStatus::DEGRADED;
      message = "High latency: " + std::to_string(avg_latency_ms) + " ms";
    }
  }
  
  exchange_health_[exchange] = HealthInfo(status, message);
  
  // Trigger alert if status changed
  if (health_alert_callback_) {
    health_alert_callback_(exchange, status);
  }
}

void ExchangeMonitor::check_performance_thresholds(const std::string& exchange) {
  auto metrics_it = exchange_metrics_.find(exchange);
  if (metrics_it == exchange_metrics_.end()) return;
  
  const auto& metrics = metrics_it->second;
  
  // Check if performance is degrading
  double success_rate = metrics.get_success_rate();
  if (success_rate < 0.7 && metrics.total_orders.load() > 10) {
    if (performance_alert_callback_) {
      performance_alert_callback_(exchange, "Success rate below 70%");
    }
  }
  
  double avg_latency_ms = metrics.get_avg_latency_us() / 1000.0;
  if (avg_latency_ms > 500 && metrics.latency_samples.load() > 5) {
    if (performance_alert_callback_) {
      performance_alert_callback_(exchange, "Average latency above 500ms");
    }
  }
}
