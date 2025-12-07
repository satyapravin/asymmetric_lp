#include "abstract_strategy.hpp"
#include "../../utils/logging/log_helper.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <atomic>

void AbstractStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    LOG_INFO_COMP("STRATEGY", "Starting strategy: " + name_);
    running_.store(true);
    
    // Call strategy-specific startup
    on_startup();
}

void AbstractStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO_COMP("STRATEGY", "Stopping strategy: " + name_);
    running_.store(false);
    
    // Cancel all pending orders via order canceller callback
    std::vector<std::string> orders_to_cancel;
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        for (const auto& [cl_ord_id, order] : pending_orders_) {
            orders_to_cancel.push_back(cl_ord_id);
        }
    }
    
    // Actually cancel orders via callback (set by StrategyContainer)
    if (order_canceller_ && !orders_to_cancel.empty()) {
        LOG_INFO_COMP("STRATEGY", "Cancelling " + std::to_string(orders_to_cancel.size()) + " pending orders");
        for (const auto& cl_ord_id : orders_to_cancel) {
            LOG_DEBUG_COMP("STRATEGY", "Cancelling order: " + cl_ord_id);
            order_canceller_(cl_ord_id);
        }
    }
    
    // Clear pending orders
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        pending_orders_.clear();
    }
    
    // Call strategy-specific shutdown
    on_shutdown();
}

std::string AbstractStrategy::generate_order_id() const {
    // Use atomic counter to ensure uniqueness across all instances
    static std::atomic<uint64_t> order_id_counter_{0};
    
    uint64_t counter = order_id_counter_.fetch_add(1, std::memory_order_relaxed);
    
    std::ostringstream oss;
    oss << name_ << "_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << counter;
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

// Order placement methods
bool AbstractStrategy::send_order(const std::string& cl_ord_id,
                                 const std::string& symbol,
                                 proto::Side side,
                                 proto::OrderType type,
                                 double qty,
                                 double price) {
    if (order_sender_) {
        return order_sender_(cl_ord_id, symbol, side, type, qty, price);
    }
    LOG_ERROR_COMP("STRATEGY", "No order sender callback set");
    return false;
}

bool AbstractStrategy::cancel_order(const std::string& cl_ord_id) {
    if (order_canceller_) {
        return order_canceller_(cl_ord_id);
    }
    LOG_ERROR_COMP("STRATEGY", "No order canceller callback set");
    return false;
}

bool AbstractStrategy::modify_order(const std::string& cl_ord_id,
                                    double new_price,
                                    double new_qty) {
    if (order_modifier_) {
        return order_modifier_(cl_ord_id, new_price, new_qty);
    }
    LOG_ERROR_COMP("STRATEGY", "No order modifier callback set");
    return false;
}
