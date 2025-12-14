#include "mini_oms.hpp"
#include "zmq_oms_adapter.hpp"
#include "../utils/logging/logger.hpp"
#include "../utils/exchange/exchange_symbol_registry.hpp"
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
    
    logging::Logger logger("MINI_OMS");
    logger.info("Starting Mini OMS with state management");
    running_.store(true);
}

void MiniOMS::stop() {
    if (!running_.load()) {
        return;
    }
    
    logging::Logger logger("MINI_OMS");
    logger.info("Stopping Mini OMS - cancelling all pending orders");
    running_.store(false);
    
    // Collect orders to cancel (release lock before cancelling to avoid deadlock)
    std::vector<std::string> orders_to_cancel;
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        for (const auto& [cl_ord_id, order_info] : orders_) {
            if (order_info.state == OrderState::PENDING || 
                order_info.state == OrderState::ACKNOWLEDGED ||
                order_info.state == OrderState::PARTIALLY_FILLED) {
                orders_to_cancel.push_back(cl_ord_id);
            }
        }
    }
    
    // Actually cancel orders via adapter
    if (oms_adapter_ && !orders_to_cancel.empty()) {
        logger.info("Cancelling " + std::to_string(orders_to_cancel.size()) + " pending orders");
        for (const auto& cl_ord_id : orders_to_cancel) {
            logger.debug("Cancelling order: " + cl_ord_id);
            // Extract exchange from order info if available
            std::string exchange = "";
            {
                std::lock_guard<std::mutex> lock(orders_mutex_);
                auto it = orders_.find(cl_ord_id);
                if (it != orders_.end()) {
                    exchange = it->second.exch;
                }
            }
            oms_adapter_->cancel_order(cl_ord_id, exchange);
        }
    } else if (orders_to_cancel.empty()) {
        logger.debug("No pending orders to cancel");
    } else {
        logger.warn("No OMS adapter available - cannot cancel orders");
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
    
    logging::Logger logger("MINI_OMS");
    
    // Validate parameters
    if (qty <= 0.0) {
        logger.error("Invalid order quantity: " + std::to_string(qty));
        return false;
    }
    
    if (type == proto::LIMIT && price <= 0.0) {
        logger.error("Invalid price for limit order: " + std::to_string(price));
        return false;
    }
    
    // Validate order parameters using exchange symbol registry
    // Note: Strategy should have already rounded prices/sizes, so we only validate here
    auto& registry = ExchangeSymbolRegistry::get_instance();
    if (!registry.validate_only(exchange_name_, symbol, qty, price)) {
        logger.error("Order validation failed for: " + cl_ord_id);
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
    order_info.exch = exchange_name_; // Store exchange name
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
        std::stringstream ss;
        ss << "Sending order: " << cl_ord_id << " " << symbol 
           << " " << (side == proto::BUY ? "BUY" : "SELL")
           << " " << qty << " @ " << price;
        logger.debug(ss.str());
        
        // Actually send order via adapter
        bool sent = oms_adapter_->send_order(cl_ord_id, exchange_name_, symbol, 
                                            (side == proto::BUY ? 0 : 1), 
                                            (type == proto::MARKET ? 1 : 0), 
                                            qty, price);
        if (!sent) {
            logger.error("Failed to send order via ZMQ adapter: " + cl_ord_id);
            // Update state to REJECTED on failure
            update_order_state(cl_ord_id, OrderState::REJECTED, "Failed to send via ZMQ");
            statistics_.rejected_orders.fetch_add(1);
            statistics_.pending_orders.fetch_sub(1);
            return false;
        }
        
        // Notify state change
        notify_order_state_change(order_info);
        return true;
    }
    
    logger.error("No OMS adapter available");
    // Update state to REJECTED if no adapter
    update_order_state(cl_ord_id, OrderState::REJECTED, "No OMS adapter available");
    statistics_.rejected_orders.fetch_add(1);
    statistics_.pending_orders.fetch_sub(1);
    return false;
}

bool MiniOMS::cancel_order(const std::string& cl_ord_id) {
    if (!running_.load()) {
        return false;
    }
    
    std::string exchange;
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(cl_ord_id);
        if (it == orders_.end()) {
            logging::Logger logger("MINI_OMS");
            logger.error("Order not found: " + cl_ord_id);
            return false;
        }
        
        OrderStateInfo& order_info = it->second;
        
        // Check if order can be cancelled
        if (!is_valid_order_transition(cl_ord_id, OrderState::CANCELLED)) {
            logging::Logger logger("MINI_OMS");
            std::stringstream ss;
            ss << "Cannot cancel order in state: " << to_string(order_info.state);
            logger.warn(ss.str());
            return false;
        }
        
        // Get exchange name from order info
        exchange = order_info.exch;
        if (exchange.empty()) {
            exchange = exchange_name_; // Fallback to default exchange name
        }
        // lock automatically unlocks when it goes out of scope
    }
    
    // Send cancel request via ZMQ adapter
    if (oms_adapter_) {
        logging::Logger logger("MINI_OMS");
        logger.debug("Cancelling order: " + cl_ord_id + " on exchange: " + exchange);
        
        // Actually send cancel via adapter
        bool cancelled = oms_adapter_->cancel_order(cl_ord_id, exchange);
        if (!cancelled) {
            logger.error("Failed to send cancel request via ZMQ adapter: " + cl_ord_id);
            // Don't update state if cancel request failed
            return false;
        }
        
        // Don't update state to CANCELLED yet - wait for exchange confirmation via order event
        // This prevents race conditions where cancel fails but state is already CANCELLED
        // The order event will update the state when exchange confirms cancellation
        logger.debug("Cancel request sent successfully - waiting for exchange confirmation");
        return true;
    }
    
    logging::Logger logger("MINI_OMS");
    logger.error("No OMS adapter available for cancel");
    return false;
}

bool MiniOMS::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    if (!running_.load()) {
        return false;
    }
    
    // Validate new parameters
    if (new_qty <= 0.0) {
        logging::Logger logger("MINI_OMS");
        logger.error("Invalid new quantity for modify: " + std::to_string(new_qty));
        return false;
    }
    
    if (new_price <= 0.0) {
        logging::Logger logger("MINI_OMS");
        logger.error("Invalid new price for modify: " + std::to_string(new_price));
        return false;
    }
    
    std::string exchange;
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(cl_ord_id);
        if (it == orders_.end()) {
            logging::Logger logger("MINI_OMS");
            logger.error("Order not found: " + cl_ord_id);
            return false;
        }
        
        OrderStateInfo& order_info = it->second;
        
        // Check if order can be modified
        if (order_info.state != OrderState::ACKNOWLEDGED && 
            order_info.state != OrderState::PARTIALLY_FILLED) {
            logging::Logger logger("MINI_OMS");
            std::stringstream ss;
            ss << "Cannot modify order in state: " << to_string(order_info.state);
            logger.warn(ss.str());
            return false;
        }
        
        exchange = order_info.exch;
        if (exchange.empty()) {
            exchange = exchange_name_; // Fallback to default exchange name
        }
    }
    
    // Send modify request via ZMQ adapter
    if (oms_adapter_) {
        logging::Logger logger("MINI_OMS");
        std::stringstream ss;
        ss << "Modifying order: " << cl_ord_id << " new_price=" << new_price << " new_qty=" << new_qty;
        logger.debug(ss.str());
        
        // Actually send modify via adapter
        bool modified = oms_adapter_->modify_order(cl_ord_id, exchange, new_price, new_qty);
        if (!modified) {
            logger.error("Failed to send modify request via ZMQ adapter: " + cl_ord_id);
            return false;
        }
        
        // Update order details locally (actual modification confirmed via order event)
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(cl_ord_id);
            if (it != orders_.end()) {
                it->second.price = new_price;
                it->second.qty = new_qty;
                it->second.last_update_time = std::chrono::system_clock::now();
            }
        }
        
        return true;
    }
    
    logging::Logger logger("MINI_OMS");
    logger.error("No OMS adapter available for modify");
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
            logging::Logger logger("MINI_OMS");
            logger.error("Unknown event type: " + std::to_string(static_cast<int>(order_event.event_type())));
            return;
    }
    
    // Update order state and store exchange_order_id if present
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(cl_ord_id);
        if (it != orders_.end()) {
            // Store exchange_order_id when available (usually in ACK event)
            if (!order_event.exch_order_id().empty()) {
                it->second.exchange_order_id = order_event.exch_order_id();
            }
        }
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
    
    logging::Logger logger("MINI_OMS");
    std::stringstream ss;
    ss << "Trade execution: " << trade.symbol() << " " << trade.qty() << " @ " << trade.price();
    logger.debug(ss.str());
}

void MiniOMS::update_order_state(const std::string& cl_ord_id, OrderState new_state, 
                                const std::string& reason, double fill_qty, double fill_price) {
    OrderStateInfo order_info_copy;
    OrderState old_state;
    bool order_found = false;
    
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(cl_ord_id);
        if (it == orders_.end()) {
            logging::Logger logger("MINI_OMS");
            logger.error("Order not found for state update: " + cl_ord_id);
            return;
        }
        
        OrderStateInfo& order_info = it->second;
        old_state = order_info.state;
        
        // Validate transition
        if (!OrderStateMachine::isValidTransition(old_state, new_state)) {
            logging::Logger logger("MINI_OMS");
            std::stringstream ss;
            ss << "Invalid state transition from " << to_string(old_state) << " to " << to_string(new_state);
            logger.error(ss.str());
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
        
        // Copy order info while holding lock (for callback)
        order_info_copy = order_info;
        order_found = true;
        
        logging::Logger logger("MINI_OMS");
        std::stringstream ss;
        ss << "Order " << cl_ord_id << " state: " << to_string(old_state) << " -> " << to_string(new_state);
        logger.debug(ss.str());
    }  // Release orders_mutex_ BEFORE calling callback to prevent deadlock
    
    // Notify callback AFTER releasing lock (prevents deadlock if callback calls back into MiniOMS)
    if (order_found) {
        notify_order_state_change(order_info_copy);
    }
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
    // Use atomic counter to ensure uniqueness across all instances
    static std::atomic<uint64_t> order_id_counter_{0};
    
    uint64_t counter = order_id_counter_.fetch_add(1, std::memory_order_relaxed);
    
    std::ostringstream oss;
    oss << "MM_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << counter;
    return oss.str();
}
