#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <functional>
#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"
#include "../proto/position.pb.h"

// Forward declarations for ZMQ adapters
class ZmqOMSAdapter;
class ZmqMDSAdapter;
class ZmqPMSAdapter;

/**
 * Abstract Strategy Base Class
 * 
 * Provides common interfaces and implementations for all trading strategies.
 * Strategies inherit from this class and implement the pure virtual methods.
 * Each trader process runs exactly one strategy instance.
 */
class AbstractStrategy {
public:
    // Strategy lifecycle
    virtual ~AbstractStrategy() = default;
    
    // Pure virtual methods that must be implemented by concrete strategies
    virtual void on_market_data(const proto::OrderBookSnapshot& orderbook) = 0;
    virtual void on_order_event(const proto::OrderEvent& order_event) = 0;
    virtual void on_position_update(const proto::PositionUpdate& position) = 0;
    virtual void on_trade_execution(const proto::Trade& trade) = 0;
    
    // Optional virtual methods with default implementations
    virtual void on_startup() {}
    virtual void on_shutdown() {}
    virtual void on_error(const std::string& error_message) {}
    
    // Strategy configuration
    virtual void set_symbol(const std::string& symbol) { symbol_ = symbol; }
    virtual void set_exchange(const std::string& exchange) { exchange_ = exchange; }
    virtual void set_enabled(bool enabled) { enabled_.store(enabled); }
    
    // Getters
    const std::string& get_symbol() const { return symbol_; }
    const std::string& get_exchange() const { return exchange_; }
    bool is_enabled() const { return enabled_.load(); }
    const std::string& get_name() const { return name_; }
    
    // Strategy lifecycle management
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const { return running_.load(); }
    
    // Risk management
    virtual void set_max_position_size(double max_size) { max_position_size_ = max_size; }
    virtual void set_max_order_size(double max_size) { max_order_size_ = max_size; }
    virtual void set_max_daily_loss(double max_loss) { max_daily_loss_ = max_loss; }
    
    // Performance metrics
    struct StrategyMetrics {
        std::atomic<uint64_t> orders_sent{0};
        std::atomic<uint64_t> orders_filled{0};
        std::atomic<uint64_t> orders_cancelled{0};
        std::atomic<uint64_t> orders_rejected{0};
        std::atomic<double> total_pnl{0.0};
        std::atomic<double> daily_pnl{0.0};
        std::atomic<uint64_t> market_data_updates{0};
        std::atomic<uint64_t> position_updates{0};
        
        void reset_daily() {
            daily_pnl.store(0.0);
            orders_sent.store(0);
            orders_filled.store(0);
            orders_cancelled.store(0);
            orders_rejected.store(0);
        }
    };
    
    const StrategyMetrics& get_metrics() const { return metrics_; }
    
    // Event callbacks for external monitoring
    using OrderCallback = std::function<void(const proto::OrderEvent&)>;
    using PositionCallback = std::function<void(const proto::PositionUpdate&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    void set_order_callback(OrderCallback callback) { order_callback_ = callback; }
    void set_position_callback(PositionCallback callback) { position_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }

protected:
    // Constructor for derived classes
    AbstractStrategy(const std::string& name) 
        : name_(name), enabled_(true), running_(false) {}
    
    // Common utility methods
    std::string generate_order_id() const;
    bool is_valid_order_size(double qty) const;
    bool is_valid_price(double price) const;
    bool is_within_risk_limits(double order_value) const;
    
    // Order tracking
    struct PendingOrder {
        std::string cl_ord_id;
        std::string symbol;
        proto::Side side;
        proto::OrderType type;
        double qty;
        double price;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::map<std::string, PendingOrder> pending_orders_;
    std::mutex orders_mutex_;
    
    // Risk management
    double max_position_size_{1000.0};
    double max_order_size_{100.0};
    double max_daily_loss_{10000.0};
    
    // Strategy state
    std::string name_;
    std::string symbol_;
    std::string exchange_;
    std::atomic<bool> enabled_;
    std::atomic<bool> running_;
    
    // Performance metrics
    mutable StrategyMetrics metrics_;
    
    // External callbacks
    OrderCallback order_callback_;
    PositionCallback position_callback_;
    ErrorCallback error_callback_;
};
