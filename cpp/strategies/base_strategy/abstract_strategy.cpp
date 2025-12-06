#include "abstract_strategy.hpp"
#include "../../utils/logging/log_helper.hpp"
#include <random>
#include <sstream>
#include <iomanip>

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
    
    // Cancel all pending orders
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        for (const auto& [cl_ord_id, order] : pending_orders_) {
            // Note: Order cancellation should be handled by the container
            LOG_INFO_COMP("STRATEGY", "Pending order to cancel: " + cl_ord_id);
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
