#pragma once
#include "../../strategies/base_strategy/abstract_strategy.hpp"
#include <iostream>
#include <atomic>
#include <mutex>
#include <vector>

namespace integration_test {

/**
 * Dummy Strategy for Integration Testing
 * 
 * This strategy subscribes to BTCUSDT orderbook updates and tracks:
 * - Number of orderbook updates received
 * - Latest bid/ask prices
 * - Connection status
 */
class DummyStrategy : public AbstractStrategy {
public:
    DummyStrategy() : 
        orderbook_count_(0),
        last_bid_(0.0),
        last_ask_(0.0),
        connected_(false) {
        std::cout << "[DUMMY_STRATEGY] Created" << std::endl;
    }
    
    ~DummyStrategy() override {
        std::cout << "[DUMMY_STRATEGY] Destroyed" << std::endl;
    }

    // AbstractStrategy interface
    void on_market_data_update(const proto::OrderBookSnapshot& orderbook) override {
        std::lock_guard<std::mutex> lock(mutex_);
        orderbook_count_++;
        
        if (orderbook.bids_size() > 0) {
            last_bid_ = orderbook.bids(0).price();
        }
        if (orderbook.asks_size() > 0) {
            last_ask_ = orderbook.asks(0).price();
        }
        
        std::cout << "[DUMMY_STRATEGY] Orderbook update #" << orderbook_count_ 
                  << " - Bid: " << last_bid_ << ", Ask: " << last_ask_ << std::endl;
    }
    
    void on_trade_update(const proto::Trade& trade) override {
        std::cout << "[DUMMY_STRATEGY] Trade update: " << trade.symbol() 
                  << " @ " << trade.price() << " qty: " << trade.quantity() << std::endl;
    }
    
    void on_order_update(const proto::OrderEvent& order_event) override {
        std::cout << "[DUMMY_STRATEGY] Order update: " << order_event.cl_ord_id() 
                  << " status: " << proto::OrderEventType_Name(order_event.event_type()) << std::endl;
    }
    
    void on_position_update(const proto::Position& position) override {
        std::cout << "[DUMMY_STRATEGY] Position update: " << position.symbol() 
                  << " size: " << position.size() << std::endl;
    }
    
    void on_balance_update(const proto::AccountBalance& balance) override {
        std::cout << "[DUMMY_STRATEGY] Balance update: " << balance.asset() 
                  << " free: " << balance.free() << std::endl;
    }
    
    void on_connection_status(bool connected) override {
        connected_ = connected;
        std::cout << "[DUMMY_STRATEGY] Connection status: " << (connected ? "CONNECTED" : "DISCONNECTED") << std::endl;
    }
    
    void on_error(const std::string& error_message) override {
        std::cout << "[DUMMY_STRATEGY] Error: " << error_message << std::endl;
    }
    
    // Strategy lifecycle
    bool initialize() override {
        std::cout << "[DUMMY_STRATEGY] Initialized" << std::endl;
        return true;
    }
    
    void start() override {
        std::cout << "[DUMMY_STRATEGY] Started" << std::endl;
    }
    
    void stop() override {
        std::cout << "[DUMMY_STRATEGY] Stopped" << std::endl;
    }
    
    // Test interface
    int get_orderbook_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return orderbook_count_;
    }
    
    double get_last_bid() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_bid_;
    }
    
    double get_last_ask() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_ask_;
    }
    
    bool is_connected() const {
        return connected_;
    }
    
    void reset_counters() {
        std::lock_guard<std::mutex> lock(mutex_);
        orderbook_count_ = 0;
        last_bid_ = 0.0;
        last_ask_ = 0.0;
    }

private:
    mutable std::mutex mutex_;
    std::atomic<int> orderbook_count_;
    std::atomic<double> last_bid_;
    std::atomic<double> last_ask_;
    std::atomic<bool> connected_;
};

} // namespace integration_test
