#include "abstract_strategy.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

void AbstractStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    std::cout << "[STRATEGY] Starting strategy: " << name_ << std::endl;
    running_.store(true);
    
    // Call strategy-specific startup
    on_startup();
}

void AbstractStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[STRATEGY] Stopping strategy: " << name_ << std::endl;
    running_.store(false);
    
    // Cancel all pending orders
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        for (const auto& [cl_ord_id, order] : pending_orders_) {
            // Note: Order cancellation should be handled by the container
            std::cout << "[STRATEGY] Pending order to cancel: " << cl_ord_id << std::endl;
        }
        pending_orders_.clear();
    }
    
    // Call strategy-specific shutdown
    on_shutdown();
}

std::string AbstractStrategy::generate_order_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << name_ << "_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << dis(gen);
    return oss.str();
}

bool AbstractStrategy::is_valid_order_size(double qty) const {
    return qty > 0.0 && qty <= max_order_size_;
}

bool AbstractStrategy::is_valid_price(double price) const {
    return price > 0.0;
}

bool AbstractStrategy::is_within_risk_limits(double order_value) const {
    return order_value <= max_position_size_;
}
