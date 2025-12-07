#pragma once

/**
 * Health Check Utilities
 * 
 * Provides standardized health checking for system components
 */

#include <string>
#include <chrono>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>

namespace health {

/**
 * Health status levels
 */
enum class HealthStatus {
    HEALTHY,      // Component is operating normally
    DEGRADED,     // Component is operating but with reduced functionality
    UNHEALTHY,    // Component is not operating correctly
    UNKNOWN       // Health status cannot be determined
};

/**
 * Health check result
 */
struct HealthCheckResult {
    HealthStatus status;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> details;  // Additional context
    
    HealthCheckResult() 
        : status(HealthStatus::UNKNOWN)
        , timestamp(std::chrono::system_clock::now()) {}
    
    HealthCheckResult(HealthStatus s, const std::string& msg)
        : status(s)
        , message(msg)
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * Health check function type
 */
using HealthCheckFunction = std::function<HealthCheckResult()>;

/**
 * Health checker for components
 */
class HealthChecker {
public:
    HealthChecker(const std::string& component_name)
        : component_name_(component_name)
        , last_check_time_(std::chrono::system_clock::now())
        , check_interval_ms_(5000)  // Default: check every 5 seconds
    {}
    
    /**
     * Register a health check function
     */
    void register_check(const std::string& check_name, HealthCheckFunction check_func) {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_[check_name] = check_func;
    }
    
    /**
     * Perform all registered health checks
     */
    HealthCheckResult check() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        HealthStatus overall_status = HealthStatus::HEALTHY;
        std::string overall_message = "All checks passed";
        std::map<std::string, std::string> details;
        
        for (const auto& [name, check_func] : checks_) {
            try {
                auto result = check_func();
                details[name] = result.message;
                
                // Overall status is worst of all checks
                if (result.status == HealthStatus::UNHEALTHY) {
                    overall_status = HealthStatus::UNHEALTHY;
                    overall_message = "Check '" + name + "' failed: " + result.message;
                } else if (result.status == HealthStatus::DEGRADED && 
                          overall_status == HealthStatus::HEALTHY) {
                    overall_status = HealthStatus::DEGRADED;
                    overall_message = "Check '" + name + "' degraded: " + result.message;
                }
            } catch (const std::exception& e) {
                details[name] = "Exception: " + std::string(e.what());
                overall_status = HealthStatus::UNHEALTHY;
                overall_message = "Check '" + name + "' threw exception: " + std::string(e.what());
            }
        }
        
        last_check_time_ = std::chrono::system_clock::now();
        
        HealthCheckResult result(overall_status, overall_message);
        result.details = details;
        return result;
    }
    
    /**
     * Get last check time
     */
    std::chrono::system_clock::time_point get_last_check_time() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_check_time_;
    }
    
    /**
     * Set check interval
     */
    void set_check_interval(int milliseconds) {
        check_interval_ms_ = milliseconds;
    }
    
    /**
     * Check if it's time to run health checks
     */
    bool should_check() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_check_time_).count();
        return elapsed >= check_interval_ms_;
    }
    
private:
    std::string component_name_;
    std::map<std::string, HealthCheckFunction> checks_;
    mutable std::mutex mutex_;
    std::chrono::system_clock::time_point last_check_time_;
    int check_interval_ms_;
};

/**
 * Helper function to create a simple health check
 */
inline HealthCheckFunction make_simple_check(
    std::function<bool()> condition,
    const std::string& success_msg = "OK",
    const std::string& failure_msg = "Failed") {
    
    return [condition, success_msg, failure_msg]() -> HealthCheckResult {
        bool ok = condition();
        return HealthCheckResult(
            ok ? HealthStatus::HEALTHY : HealthStatus::UNHEALTHY,
            ok ? success_msg : failure_msg
        );
    };
}

} // namespace health

