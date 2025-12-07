#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include "../utils/oms/order_state.hpp"
#include "../utils/oms/order.hpp"
#include "../utils/oms/types.hpp"
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"

// Forward declarations
class ZmqOMSAdapter;
class ZmqMDSAdapter;
class ZmqPMSAdapter;

/**
 * Mini OMS with State Management
 * 
 * Combines order state tracking, state machine, and ZMQ adapter routing.
 * Provides a complete order management system for the strategy container.
 */
class MiniOMS {
public:
    using OrderStateCallback = std::function<void(const OrderStateInfo& order_info)>;
    using OrderEventCallback = std::function<void(const OrderEvent& order_event)>;
    
    MiniOMS();
    ~MiniOMS() = default;
    
    // ZMQ adapter setup
    void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter);
    void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter);
    void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter);
    
    // Exchange configuration
    void set_exchange_name(const std::string& exchange_name) { exchange_name_ = exchange_name; }
    
    // Order management
    bool send_order(const std::string& cl_ord_id,
                   const std::string& symbol,
                   proto::Side side,
                   proto::OrderType type,
                   double qty,
                   double price = 0.0);
    
    bool cancel_order(const std::string& cl_ord_id);
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty);
    
    // Order state queries
    OrderStateInfo get_order_state(const std::string& cl_ord_id);
    std::vector<OrderStateInfo> get_active_orders();
    std::vector<OrderStateInfo> get_all_orders();
    std::vector<OrderStateInfo> get_orders_by_symbol(const std::string& symbol);
    std::vector<OrderStateInfo> get_orders_by_state(OrderState state);
    
    // Event handling
    void on_order_event(const proto::OrderEvent& order_event);
    void on_trade_execution(const proto::Trade& trade);
    
    // Callbacks
    void set_order_state_callback(OrderStateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        order_state_callback_ = callback;
    }
    void set_order_event_callback(OrderEventCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        order_event_callback_ = callback;
    }
    
    // Statistics
    struct OrderStatistics {
        std::atomic<uint64_t> total_orders{0};
        std::atomic<uint64_t> pending_orders{0};
        std::atomic<uint64_t> active_orders{0};
        std::atomic<uint64_t> filled_orders{0};
        std::atomic<uint64_t> cancelled_orders{0};
        std::atomic<uint64_t> rejected_orders{0};
        std::atomic<double> total_volume{0.0};
        std::atomic<double> total_pnl{0.0};
        
        void reset() {
            total_orders.store(0);
            pending_orders.store(0);
            active_orders.store(0);
            filled_orders.store(0);
            cancelled_orders.store(0);
            rejected_orders.store(0);
            total_volume.store(0.0);
            total_pnl.store(0.0);
        }
    };
    
    const OrderStatistics& get_statistics() const { return statistics_; }
    
    // Lifecycle
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    // Order tracking
    std::unordered_map<std::string, OrderStateInfo> orders_;
    mutable std::mutex orders_mutex_;
    
    // ZMQ adapters
    std::shared_ptr<ZmqOMSAdapter> oms_adapter_;
    std::shared_ptr<ZmqMDSAdapter> mds_adapter_;
    std::shared_ptr<ZmqPMSAdapter> pms_adapter_;
    
    // Exchange configuration
    std::string exchange_name_;
    
    // State management
    std::atomic<bool> running_;
    OrderStatistics statistics_;
    
    // Callbacks (protected by mutex for thread safety)
    mutable std::mutex callback_mutex_;
    OrderStateCallback order_state_callback_;
    OrderEventCallback order_event_callback_;
    
    // Helper methods
    void update_order_state(const std::string& cl_ord_id, OrderState new_state, 
                           const std::string& reason = "", double fill_qty = 0.0, 
                           double fill_price = 0.0);
    void notify_order_state_change(const OrderStateInfo& order_info);
    std::string generate_order_id() const;
    bool is_valid_order_transition(const std::string& cl_ord_id, OrderState new_state);
};
