#include "mini_oms.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

MiniOMS::MiniOMS() : running_(false) {
    statistics_.reset();
}

void MiniOMS::set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) {
    oms_adapter_ = adapter;
}

void MiniOMS::set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) {
    mds_adapter_ = adapter;
}

void MiniOMS::set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) {
    pms_adapter_ = adapter;
}

void MiniOMS::start() {
    if (running_.load()) {
        return;
    }
    
    std::cout << "[MINI_OMS] Starting Mini OMS with state management" << std::endl;
    running_.store(true);
}

void MiniOMS::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[MINI_OMS] Stopping Mini OMS" << std::endl;
    running_.store(false);
    
    // Cancel all pending orders
    std::lock_guard<std::mutex> lock(orders_mutex_);
    for (const auto& [cl_ord_id, order_info] : orders_) {
        if (order_info.state == OrderState::PENDING || 
            order_info.state == OrderState::ACKNOWLEDGED) {
            std::cout << "[MINI_OMS] Cancelling pending order: " << cl_ord_id << std::endl;
            // Note: Actual cancellation would be handled by ZMQ adapter
        }
    }
}

bool MiniOMS::send_order(const std::string& cl_ord_id,
                        const std::string& symbol,
                        proto::Side side,
                        proto::OrderType type,
                        double qty,
                        double price) {
    if (!running_.load()) {
        return false;
    }
    
    // Validate parameters
    if (qty <= 0.0) {
        std::cerr << "[MINI_OMS] Invalid order quantity: " << qty << std::endl;
        return false;
    }
    
    if (type == proto::LIMIT && price <= 0.0) {
        std::cerr << "[MINI_OMS] Invalid price for limit order: " << price << std::endl;
        return false;
    }
    
    // Create order state info
    OrderStateInfo order_info;
    order_info.cl_ord_id = cl_ord_id;
    order_info.symbol = symbol;
    order_info.side = (side == proto::BUY) ? Side::Buy : Side::Sell;
    order_info.qty = qty;
    order_info.price = price;
    order_info.is_market = (type == proto::MARKET);
    order_info.state = OrderState::PENDING;
    order_info.created_time = std::chrono::system_clock::now();
    order_info.last_update_time = order_info.created_time;
    
    // Store order
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        orders_[cl_ord_id] = order_info;
    }
    
    // Update statistics
    statistics_.total_orders.fetch_add(1);
    statistics_.pending_orders.fetch_add(1);
    
    // Send order via ZMQ adapter
    if (oms_adapter_) {
        // Note: This would need to be implemented in ZmqOMSAdapter
        std::cout << "[MINI_OMS] Sending order: " << cl_ord_id << " " << symbol 
                  << " " << (side == proto::BUY ? "BUY" : "SELL")
                  << " " << qty << " @ " << price << std::endl;
        
        // Notify state change
        notify_order_state_change(order_info);
        return true;
    }
    
    std::cerr << "[MINI_OMS] No OMS adapter available" << std::endl;
    return false;
}

bool MiniOMS::cancel_order(const std::string& cl_ord_id) {
    if (!running_.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(cl_ord_id);
    if (it == orders_.end()) {
        std::cerr << "[MINI_OMS] Order not found: " << cl_ord_id << std::endl;
        return false;
    }
    
    OrderStateInfo& order_info = it->second;
    
    // Check if order can be cancelled
    if (!is_valid_order_transition(cl_ord_id, OrderState::CANCELLED)) {
        std::cerr << "[MINI_OMS] Cannot cancel order in state: " 
                  << to_string(order_info.state) << std::endl;
        return false;
    }
    
    // Update state
    update_order_state(cl_ord_id, OrderState::CANCELLED, "Cancelled by user");
    
    // Send cancel request via ZMQ adapter
    if (oms_adapter_) {
        std::cout << "[MINI_OMS] Cancelling order: " << cl_ord_id << std::endl;
        // Note: This would need to be implemented in ZmqOMSAdapter
        return true;
    }
    
    return false;
}

bool MiniOMS::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    if (!running_.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(cl_ord_id);
    if (it == orders_.end()) {
        std::cerr << "[MINI_OMS] Order not found: " << cl_ord_id << std::endl;
        return false;
    }
    
    OrderStateInfo& order_info = it->second;
    
    // Check if order can be modified
    if (order_info.state != OrderState::ACKNOWLEDGED) {
        std::cerr << "[MINI_OMS] Cannot modify order in state: " 
                  << to_string(order_info.state) << std::endl;
        return false;
    }
    
    // Update order details
    order_info.price = new_price;
    order_info.qty = new_qty;
    order_info.last_update_time = std::chrono::system_clock::now();
    
    // Send modify request via ZMQ adapter
    if (oms_adapter_) {
        std::cout << "[MINI_OMS] Modifying order: " << cl_ord_id 
                  << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
        // Note: This would need to be implemented in ZmqOMSAdapter
        return true;
    }
    
    return false;
}

OrderStateInfo MiniOMS::get_order_state(const std::string& cl_ord_id) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(cl_ord_id);
    if (it != orders_.end()) {
        return it->second;
    }
    
    // Return empty state for non-existent order
    OrderStateInfo empty_state;
    empty_state.cl_ord_id = cl_ord_id;
    return empty_state;
}

std::vector<OrderStateInfo> MiniOMS::get_active_orders() {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    std::vector<OrderStateInfo> active_orders;
    
    for (const auto& [cl_ord_id, order_info] : orders_) {
        if (order_info.state == OrderState::PENDING || 
            order_info.state == OrderState::ACKNOWLEDGED ||
            order_info.state == OrderState::PARTIALLY_FILLED) {
            active_orders.push_back(order_info);
        }
    }
    
    return active_orders;
}

std::vector<OrderStateInfo> MiniOMS::get_all_orders() {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    std::vector<OrderStateInfo> all_orders;
    
    for (const auto& [cl_ord_id, order_info] : orders_) {
        all_orders.push_back(order_info);
    }
    
    return all_orders;
}

std::vector<OrderStateInfo> MiniOMS::get_orders_by_symbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    std::vector<OrderStateInfo> symbol_orders;
    
    for (const auto& [cl_ord_id, order_info] : orders_) {
        if (order_info.symbol == symbol) {
            symbol_orders.push_back(order_info);
        }
    }
    
    return symbol_orders;
}

std::vector<OrderStateInfo> MiniOMS::get_orders_by_state(OrderState state) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    std::vector<OrderStateInfo> state_orders;
    
    for (const auto& [cl_ord_id, order_info] : orders_) {
        if (order_info.state == state) {
            state_orders.push_back(order_info);
        }
    }
    
    return state_orders;
}

void MiniOMS::on_order_event(const proto::OrderEvent& order_event) {
    if (!running_.load()) {
        return;
    }
    
    std::string cl_ord_id = order_event.cl_ord_id();
    OrderState new_state;
    
    // Map proto event type to order state
    switch (order_event.event_type()) {
        case proto::OrderEventType::ACK:
            new_state = OrderState::ACKNOWLEDGED;
            break;
        case proto::OrderEventType::FILL:
            new_state = OrderState::PARTIALLY_FILLED; // Will be updated to FILLED if complete
            break;
        case proto::OrderEventType::CANCEL:
            new_state = OrderState::CANCELLED;
            break;
        case proto::OrderEventType::REJECT:
            new_state = OrderState::REJECTED;
            break;
        default:
            std::cerr << "[MINI_OMS] Unknown event type: " << order_event.event_type() << std::endl;
            return;
    }
    
    // Update order state
    update_order_state(cl_ord_id, new_state, order_event.text(), 
                      order_event.fill_qty(), order_event.fill_price());
    
    // Notify external callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (order_event_callback_) {
            // Convert proto event to legacy OrderEvent
            OrderEvent legacy_event;
            legacy_event.cl_ord_id = cl_ord_id;
            legacy_event.exch = order_event.exch();
            legacy_event.symbol = order_event.symbol();
            legacy_event.type = static_cast<OrderEventType>(order_event.event_type());
            legacy_event.fill_qty = order_event.fill_qty();
            legacy_event.fill_price = order_event.fill_price();
            legacy_event.text = order_event.text();
            legacy_event.exchange_order_id = order_event.exch_order_id();
            legacy_event.timestamp_us = order_event.timestamp_us();
            
            order_event_callback_(legacy_event);
        }
    }
}

void MiniOMS::on_trade_execution(const proto::Trade& trade) {
    if (!running_.load()) {
        return;
    }
    
    // Note: Trade proto doesn't have cl_ord_id, so we can't directly map to orders
    // This would need to be handled by matching trade_id or other fields
    // For now, we'll just update statistics
    
    // Update statistics
    double trade_value = trade.qty() * trade.price();
    double current_volume = statistics_.total_volume.load();
    statistics_.total_volume.store(current_volume + trade_value);
    
    std::cout << "[MINI_OMS] Trade execution: " << trade.symbol() 
              << " " << trade.qty() << " @ " << trade.price() << std::endl;
}

void MiniOMS::update_order_state(const std::string& cl_ord_id, OrderState new_state, 
                                const std::string& reason, double fill_qty, double fill_price) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(cl_ord_id);
    if (it == orders_.end()) {
        std::cerr << "[MINI_OMS] Order not found for state update: " << cl_ord_id << std::endl;
        return;
    }
    
    OrderStateInfo& order_info = it->second;
    OrderState old_state = order_info.state;
    
    // Validate transition
    if (!OrderStateMachine::isValidTransition(old_state, new_state)) {
        std::cerr << "[MINI_OMS] Invalid state transition from " << to_string(old_state) 
                  << " to " << to_string(new_state) << std::endl;
        return;
    }
    
    // Update state
    order_info.state = new_state;
    order_info.last_update_time = std::chrono::system_clock::now();
    
    if (!reason.empty()) {
        order_info.reject_reason = reason;
    }
    
    if (fill_qty > 0.0) {
        order_info.filled_qty += fill_qty;
        if (fill_price > 0.0) {
            // Update average fill price
            double total_value = (order_info.avg_fill_price * (order_info.filled_qty - fill_qty)) + 
                               (fill_price * fill_qty);
            order_info.avg_fill_price = total_value / order_info.filled_qty;
        }
    }
    
    // Update statistics
    switch (new_state) {
        case OrderState::ACKNOWLEDGED:
            statistics_.pending_orders.fetch_sub(1);
            statistics_.active_orders.fetch_add(1);
            break;
        case OrderState::FILLED:
            statistics_.active_orders.fetch_sub(1);
            statistics_.filled_orders.fetch_add(1);
            break;
        case OrderState::CANCELLED:
            statistics_.active_orders.fetch_sub(1);
            statistics_.cancelled_orders.fetch_add(1);
            break;
        case OrderState::REJECTED:
            statistics_.pending_orders.fetch_sub(1);
            statistics_.rejected_orders.fetch_add(1);
            break;
        default:
            break;
    }
    
    // Notify callback
    notify_order_state_change(order_info);
    
    std::cout << "[MINI_OMS] Order " << cl_ord_id << " state: " 
              << to_string(old_state) << " -> " << to_string(new_state) << std::endl;
}

void MiniOMS::notify_order_state_change(const OrderStateInfo& order_info) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (order_state_callback_) {
            order_state_callback_(order_info);
        }
    }
}

bool MiniOMS::is_valid_order_transition(const std::string& cl_ord_id, OrderState new_state) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(cl_ord_id);
    if (it == orders_.end()) {
        return false;
    }
    
    return OrderStateMachine::isValidTransition(it->second.state, new_state);
}

std::string MiniOMS::generate_order_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "MM_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << dis(gen);
    return oss.str();
}
