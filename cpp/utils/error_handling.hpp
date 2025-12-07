#pragma once

/**
 * Standardized error handling utilities
 * 
 * Provides common error handling patterns and utilities
 * for consistent error handling across the codebase.
 */

#include <string>
#include <exception>
#include <functional>
#include <optional>
#include "../utils/logging/log_helper.hpp"

namespace error_handling {

/**
 * Result type for operations that can fail
 * Similar to Rust's Result<T, E> or std::expected
 */
template<typename T>
class Result {
public:
    // Success constructor
    static Result success(T value) {
        Result result;
        result.value_ = std::move(value);
        result.success_ = true;
        return result;
    }
    
    // Error constructor
    static Result error(const std::string& error_message) {
        Result result;
        result.error_message_ = error_message;
        result.success_ = false;
        return result;
    }
    
    // Check if operation was successful
    bool is_success() const { return success_; }
    bool is_error() const { return !success_; }
    
    // Get the value (only call if is_success() == true)
    const T& value() const { 
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error Result");
        }
        return value_.value();
    }
    
    T& value() { 
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error Result");
        }
        return value_.value();
    }
    
    // Get the error message (only call if is_error() == true)
    const std::string& error() const { 
        if (success_) {
            throw std::runtime_error("Attempted to get error from success Result");
        }
        return error_message_;
    }
    
    // Convenience operators
    explicit operator bool() const { return success_; }
    const T& operator*() const { return value(); }
    T& operator*() { return value(); }
    
private:
    std::optional<T> value_;
    std::string error_message_;
    bool success_;
};

/**
 * Execute a function with exception handling and logging
 * 
 * @param func Function to execute
 * @param component_name Component name for logging
 * @param operation_name Operation name for logging
 * @return Result with return value or error message
 */
template<typename Func>
auto safe_execute(Func&& func, const std::string& component_name, const std::string& operation_name) 
    -> Result<decltype(func())> {
    try {
        auto result = func();
        return Result<decltype(result)>::success(std::move(result));
    } catch (const std::exception& e) {
        LOG_ERROR_COMP(component_name, operation_name + " failed: " + std::string(e.what()));
        return Result<decltype(func())>::error(std::string(e.what()));
    } catch (...) {
        LOG_ERROR_COMP(component_name, operation_name + " failed with unknown exception");
        return Result<decltype(func())>::error("Unknown exception");
    }
}

/**
 * Execute a void function with exception handling and logging
 * 
 * @param func Function to execute
 * @param component_name Component name for logging
 * @param operation_name Operation name for logging
 * @return true if successful, false otherwise
 */
template<typename Func>
bool safe_execute_void(Func&& func, const std::string& component_name, const std::string& operation_name) {
    try {
        func();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR_COMP(component_name, operation_name + " failed: " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR_COMP(component_name, operation_name + " failed with unknown exception");
        return false;
    }
}

/**
 * Execute a callback with exception handling
 * Prevents callback exceptions from crashing the system
 * 
 * @param callback Callback function to execute
 * @param component_name Component name for logging
 * @param operation_name Operation name for logging
 * @param args Arguments to pass to callback
 */
template<typename Callback, typename... Args>
void safe_callback(Callback&& callback, const std::string& component_name, 
                   const std::string& operation_name, Args&&... args) {
    if (!callback) {
        return;  // No callback set
    }
    
    try {
        callback(std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        LOG_ERROR_COMP(component_name, "Exception in " + operation_name + " callback: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR_COMP(component_name, "Unknown exception in " + operation_name + " callback");
    }
}

} // namespace error_handling

