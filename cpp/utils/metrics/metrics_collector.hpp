#pragma once

/**
 * Unified Metrics Collection System
 * 
 * Provides a centralized, thread-safe metrics collection interface
 * for all system components. Supports counters, gauges, histograms,
 * and timers.
 */

#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <memory>
#include "../logging/log_helper.hpp"

namespace metrics {

/**
 * Metric types
 */
enum class MetricType {
    COUNTER,    // Incrementing counter
    GAUGE,      // Current value (can go up or down)
    HISTOGRAM,  // Distribution of values
    TIMER       // Duration measurements
};

/**
 * Base metric interface
 */
class IMetric {
public:
    virtual ~IMetric() = default;
    virtual MetricType get_type() const = 0;
    virtual std::string get_name() const = 0;
    virtual std::string to_string() const = 0;
};

/**
 * Counter metric - increments only
 */
class Counter : public IMetric {
public:
    Counter(const std::string& name) : name_(name), value_(0) {}
    
    void increment(int64_t delta = 1) {
        value_.fetch_add(delta);
    }
    
    void reset() {
        value_.store(0);
    }
    
    int64_t get() const {
        return value_.load();
    }
    
    MetricType get_type() const override { return MetricType::COUNTER; }
    std::string get_name() const override { return name_; }
    
    std::string to_string() const override {
        return name_ + ": " + std::to_string(value_.load());
    }
    
private:
    std::string name_;
    std::atomic<int64_t> value_;
};

/**
 * Gauge metric - can increase or decrease
 */
class Gauge : public IMetric {
public:
    Gauge(const std::string& name) : name_(name), value_(0) {}
    
    void set(double value) {
        value_.store(value);
    }
    
    void increment(double delta = 1.0) {
        double current = value_.load();
        value_.store(current + delta);
    }
    
    void decrement(double delta = 1.0) {
        double current = value_.load();
        value_.store(current - delta);
    }
    
    double get() const {
        return value_.load();
    }
    
    MetricType get_type() const override { return MetricType::GAUGE; }
    std::string get_name() const override { return name_; }
    
    std::string to_string() const override {
        return name_ + ": " + std::to_string(value_.load());
    }
    
private:
    std::string name_;
    std::atomic<double> value_;
};

/**
 * Histogram metric - tracks distribution of values
 */
class Histogram : public IMetric {
public:
    Histogram(const std::string& name, size_t buckets = 10) 
        : name_(name), count_(0), sum_(0.0), buckets_(buckets) {}
    
    void record(double value) {
        count_.fetch_add(1);
        double current_sum = sum_.load();
        sum_.store(current_sum + value);
        
        // Simple bucket tracking (can be enhanced)
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(value);
        if (values_.size() > buckets_ * 100) {  // Keep last N samples
            values_.erase(values_.begin(), values_.begin() + buckets_ * 50);
        }
    }
    
    size_t get_count() const { return count_.load(); }
    double get_sum() const { return sum_.load(); }
    double get_mean() const {
        size_t cnt = count_.load();
        return cnt > 0 ? sum_.load() / cnt : 0.0;
    }
    
    MetricType get_type() const override { return MetricType::HISTOGRAM; }
    std::string get_name() const override { return name_; }
    
    std::string to_string() const override {
        size_t cnt = count_.load();
        return name_ + ": count=" + std::to_string(cnt) + 
               " sum=" + std::to_string(sum_.load()) +
               " mean=" + std::to_string(cnt > 0 ? sum_.load() / cnt : 0.0);
    }
    
private:
    std::string name_;
    std::atomic<size_t> count_;
    std::atomic<double> sum_;
    size_t buckets_;
    mutable std::mutex mutex_;
    std::vector<double> values_;
};

/**
 * Timer metric - measures durations
 */
class Timer : public IMetric {
public:
    Timer(const std::string& name) : name_(name), count_(0), total_us_(0) {}
    
    class ScopedTimer {
    public:
        ScopedTimer(Timer& timer) : timer_(timer), start_(std::chrono::steady_clock::now()) {}
        
        ~ScopedTimer() {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
            timer_.record(duration.count());
        }
        
    private:
        Timer& timer_;
        std::chrono::steady_clock::time_point start_;
    };
    
    void record(int64_t microseconds) {
        count_.fetch_add(1);
        total_us_.fetch_add(microseconds);
    }
    
    ScopedTimer start() {
        return ScopedTimer(*this);
    }
    
    size_t get_count() const { return count_.load(); }
    int64_t get_total_us() const { return total_us_.load(); }
    double get_mean_us() const {
        size_t cnt = count_.load();
        return cnt > 0 ? static_cast<double>(total_us_.load()) / cnt : 0.0;
    }
    
    MetricType get_type() const override { return MetricType::TIMER; }
    std::string get_name() const override { return name_; }
    
    std::string to_string() const override {
        size_t cnt = count_.load();
        return name_ + ": count=" + std::to_string(cnt) + 
               " total_us=" + std::to_string(total_us_.load()) +
               " mean_us=" + std::to_string(cnt > 0 ? static_cast<double>(total_us_.load()) / cnt : 0.0);
    }
    
private:
    std::string name_;
    std::atomic<size_t> count_;
    std::atomic<int64_t> total_us_;
};

/**
 * Centralized metrics collector
 * Thread-safe singleton for collecting metrics across the system
 */
class MetricsCollector {
public:
    static MetricsCollector& instance() {
        static MetricsCollector instance;
        return instance;
    }
    
    // Counter operations
    Counter& counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = counters_.find(name);
        if (it == counters_.end()) {
            auto counter = std::make_unique<Counter>(name);
            auto* ptr = counter.get();
            counters_[name] = std::move(counter);
            return *ptr;
        }
        return *it->second;
    }
    
    // Gauge operations
    Gauge& gauge(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = gauges_.find(name);
        if (it == gauges_.end()) {
            auto gauge = std::make_unique<Gauge>(name);
            auto* ptr = gauge.get();
            gauges_[name] = std::move(gauge);
            return *ptr;
        }
        return *it->second;
    }
    
    // Histogram operations
    Histogram& histogram(const std::string& name, size_t buckets = 10) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = histograms_.find(name);
        if (it == histograms_.end()) {
            auto hist = std::make_unique<Histogram>(name, buckets);
            auto* ptr = hist.get();
            histograms_[name] = std::move(hist);
            return *ptr;
        }
        return *it->second;
    }
    
    // Timer operations
    Timer& timer(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = timers_.find(name);
        if (it == timers_.end()) {
            auto timer = std::make_unique<Timer>(name);
            auto* ptr = timer.get();
            timers_[name] = std::move(timer);
            return *ptr;
        }
        return *it->second;
    }
    
    // Get all metrics as strings
    std::vector<std::string> get_all_metrics() const {
        std::vector<std::string> result;
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, counter] : counters_) {
            result.push_back(counter->to_string());
        }
        for (const auto& [name, gauge] : gauges_) {
            result.push_back(gauge->to_string());
        }
        for (const auto& [name, hist] : histograms_) {
            result.push_back(hist->to_string());
        }
        for (const auto& [name, timer] : timers_) {
            result.push_back(timer->to_string());
        }
        
        return result;
    }
    
    // Print all metrics
    void print_all_metrics() const {
        auto metrics = get_all_metrics();
        LOG_INFO_COMP("METRICS", "=== Metrics Summary ===");
        for (const auto& metric : metrics) {
            LOG_INFO_COMP("METRICS", metric);
        }
    }
    
    // Reset all metrics
    void reset_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, counter] : counters_) {
            counter->reset();
        }
        // Gauges, histograms, and timers don't have reset - they accumulate
    }
    
private:
    MetricsCollector() = default;
    ~MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::map<std::string, std::unique_ptr<Histogram>> histograms_;
    std::map<std::string, std::unique_ptr<Timer>> timers_;
};

// Convenience macros for easy metric access
#define METRICS_COUNTER(name) metrics::MetricsCollector::instance().counter(name)
#define METRICS_GAUGE(name) metrics::MetricsCollector::instance().gauge(name)
#define METRICS_HISTOGRAM(name) metrics::MetricsCollector::instance().histogram(name)
#define METRICS_TIMER(name) metrics::MetricsCollector::instance().timer(name)

} // namespace metrics

