#pragma once
#include <string>
#include <exception>
#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace resilience {

enum class ErrorType {
    NETWORK_ERROR,
    API_ERROR,
    RATE_LIMIT_ERROR,
    AUTHENTICATION_ERROR,
    VALIDATION_ERROR,
    SYSTEM_ERROR,
    UNKNOWN_ERROR
};

class ResilientError : public std::exception {
public:
    ResilientError(ErrorType type, const std::string& message, const std::string& context = "")
        : type_(type), message_(message), context_(context) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    ErrorType get_type() const { return type_; }
    const std::string& get_context() const { return context_; }
    
private:
    ErrorType type_;
    std::string message_;
    std::string context_;
};

class CircuitBreaker {
public:
    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Circuit is open, requests fail fast
        HALF_OPEN  // Testing if service is back
    };
    
    CircuitBreaker(int failure_threshold = 5, 
                   std::chrono::milliseconds timeout = std::chrono::seconds(60),
                   std::chrono::milliseconds retry_timeout = std::chrono::seconds(30))
        : failure_threshold_(failure_threshold)
        , timeout_(timeout)
        , retry_timeout_(retry_timeout)
        , failure_count_(0)
        , last_failure_time_(std::chrono::steady_clock::now())
        , state_(State::CLOSED) {}
    
    template<typename Func>
    auto execute(Func func) -> decltype(func()) {
        if (state_ == State::OPEN) {
            if (std::chrono::steady_clock::now() - last_failure_time_ > retry_timeout_) {
                state_ = State::HALF_OPEN;
            } else {
                throw ResilientError(ErrorType::SYSTEM_ERROR, "Circuit breaker is OPEN");
            }
        }
        
        try {
            auto result = func();
            on_success();
            return result;
        } catch (const std::exception& e) {
            on_failure();
            throw;
        }
    }
    
    State get_state() const { return state_; }
    int get_failure_count() const { return failure_count_; }
    
private:
    void on_success() {
        failure_count_ = 0;
        state_ = State::CLOSED;
    }
    
    void on_failure() {
        failure_count_++;
        last_failure_time_ = std::chrono::steady_clock::now();
        
        if (failure_count_ >= failure_threshold_) {
            state_ = State::OPEN;
        }
    }
    
    int failure_threshold_;
    std::chrono::milliseconds timeout_;
    std::chrono::milliseconds retry_timeout_;
    std::atomic<int> failure_count_;
    std::chrono::steady_clock::time_point last_failure_time_;
    std::atomic<State> state_;
};

class RetryPolicy {
public:
    RetryPolicy(int max_retries = 3, 
                std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100),
                double backoff_multiplier = 2.0,
                std::chrono::milliseconds max_delay = std::chrono::seconds(30))
        : max_retries_(max_retries)
        , initial_delay_(initial_delay)
        , backoff_multiplier_(backoff_multiplier)
        , max_delay_(max_delay) {}
    
    template<typename Func>
    auto execute(Func func) -> decltype(func()) {
        int retry_count = 0;
        std::chrono::milliseconds delay = initial_delay_;
        
        while (retry_count <= max_retries_) {
            try {
                return func();
            } catch (const ResilientError& e) {
                if (!should_retry(e.get_type()) || retry_count >= max_retries_) {
                    throw;
                }
                
                retry_count++;
                std::this_thread::sleep_for(delay);
                delay = std::min(delay * backoff_multiplier_, std::chrono::duration<double, std::milli>(max_delay_));
            } catch (const std::exception& e) {
                if (retry_count >= max_retries_) {
                    throw;
                }
                
                retry_count++;
                std::this_thread::sleep_for(delay);
                delay = std::min(delay * backoff_multiplier_, std::chrono::duration<double, std::milli>(max_delay_));
            }
        }
        
        throw ResilientError(ErrorType::SYSTEM_ERROR, "Max retries exceeded");
    }
    
private:
    bool should_retry(ErrorType type) {
        switch (type) {
            case ErrorType::NETWORK_ERROR:
            case ErrorType::RATE_LIMIT_ERROR:
                return true;
            case ErrorType::AUTHENTICATION_ERROR:
            case ErrorType::VALIDATION_ERROR:
                return false;
            default:
                return true;
        }
    }
    
    int max_retries_;
    std::chrono::milliseconds initial_delay_;
    double backoff_multiplier_;
    std::chrono::milliseconds max_delay_;
};

class DeadLetterQueue {
public:
    using MessageHandler = std::function<void(const std::string&, const std::string&)>;
    
    DeadLetterQueue(size_t max_size = 1000) : max_size_(max_size) {}
    
    void enqueue(const std::string& message, const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.size() >= max_size_) {
            queue_.pop(); // Remove oldest message
        }
        
        queue_.push({message, error, std::chrono::system_clock::now()});
    }
    
    void process_messages(MessageHandler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!queue_.empty()) {
            auto entry = queue_.front();
            queue_.pop();
            
            try {
                handler(entry.message, entry.error);
            } catch (const std::exception& e) {
                // Log error but continue processing
                std::cerr << "[DLQ] Error processing message: " << e.what() << std::endl;
            }
        }
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    struct DLQEntry {
        std::string message;
        std::string error;
        std::chrono::system_clock::time_point timestamp;
    };
    
    size_t max_size_;
    std::queue<DLQEntry> queue_;
    mutable std::mutex mutex_;
};

class ResilienceManager {
public:
    static ResilienceManager& get_instance() {
        static ResilienceManager instance;
        return instance;
    }
    
    CircuitBreaker& get_circuit_breaker(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = circuit_breakers_.find(service_name);
        if (it == circuit_breakers_.end()) {
            circuit_breakers_[service_name] = std::make_unique<CircuitBreaker>();
        }
        
        return *circuit_breakers_[service_name];
    }
    
    RetryPolicy& get_retry_policy(const std::string& operation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = retry_policies_.find(operation_name);
        if (it == retry_policies_.end()) {
            retry_policies_[operation_name] = std::make_unique<RetryPolicy>();
        }
        
        return *retry_policies_[operation_name];
    }
    
    DeadLetterQueue& get_dlq(const std::string& queue_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = dlqs_.find(queue_name);
        if (it == dlqs_.end()) {
            dlqs_[queue_name] = std::make_unique<DeadLetterQueue>();
        }
        
        return *dlqs_[queue_name];
    }
    
    void set_circuit_breaker_config(const std::string& service_name, 
                                   int failure_threshold,
                                   std::chrono::milliseconds timeout,
                                   std::chrono::milliseconds retry_timeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        circuit_breakers_[service_name] = std::make_unique<CircuitBreaker>(
            failure_threshold, timeout, retry_timeout);
    }
    
    void set_retry_policy_config(const std::string& operation_name,
                                int max_retries,
                                std::chrono::milliseconds initial_delay,
                                double backoff_multiplier,
                                std::chrono::milliseconds max_delay) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        retry_policies_[operation_name] = std::make_unique<RetryPolicy>(
            max_retries, initial_delay, backoff_multiplier, max_delay);
    }
    
private:
    ResilienceManager() = default;
    
    std::map<std::string, std::unique_ptr<CircuitBreaker>> circuit_breakers_;
    std::map<std::string, std::unique_ptr<RetryPolicy>> retry_policies_;
    std::map<std::string, std::unique_ptr<DeadLetterQueue>> dlqs_;
    mutable std::mutex mutex_;
};

// Convenience macros
#define CIRCUIT_BREAKER(service_name, func) \
    ResilienceManager::get_instance().get_circuit_breaker(service_name).execute(func)

#define RETRY_POLICY(operation_name, func) \
    ResilienceManager::get_instance().get_retry_policy(operation_name).execute(func)

#define DLQ_ENQUEUE(queue_name, message, error) \
    ResilienceManager::get_instance().get_dlq(queue_name).enqueue(message, error)

} // namespace resilience
