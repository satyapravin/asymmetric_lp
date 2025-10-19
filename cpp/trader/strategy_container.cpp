#include "strategy_container.hpp"
#include "../strategies/base_strategy/abstract_strategy.hpp"
#include <iostream>

/**
 * Strategy Container Implementation
 * 
 * Holds a single strategy instance and delegates all events to it.
 * Uses Mini OMS for order state management and ZMQ adapter routing.
 */
class StrategyContainer : public IStrategyContainer {
public:
    StrategyContainer() {
        mini_oms_ = std::make_unique<MiniOMS>();
    }
    
    ~StrategyContainer() = default;
    
    // Set the strategy instance
    void set_strategy(std::shared_ptr<AbstractStrategy> strategy) {
        strategy_ = strategy;
        // Strategy doesn't know about adapters - it delegates to container
    }
    
    // IStrategyContainer interface implementation
    void start() override {
        if (mini_oms_) {
            mini_oms_->start();
        }
        if (strategy_) {
            strategy_->start();
        }
    }
    
    void stop() override {
        if (strategy_) {
            strategy_->stop();
        }
        if (mini_oms_) {
            mini_oms_->stop();
        }
    }
    
    bool is_running() const override {
        return strategy_ ? strategy_->is_running() : false;
    }
    
    void on_market_data(const proto::OrderBookSnapshot& orderbook) override {
        if (strategy_) {
            strategy_->on_market_data(orderbook);
        }
    }
    
    void on_order_event(const proto::OrderEvent& order_event) override {
        // Update Mini OMS state first
        if (mini_oms_) {
            mini_oms_->on_order_event(order_event);
        }
        
        // Then notify strategy
        if (strategy_) {
            strategy_->on_order_event(order_event);
        }
    }
    
    void on_position_update(const proto::PositionUpdate& position) override {
        if (strategy_) {
            strategy_->on_position_update(position);
        }
    }
    
    void on_trade_execution(const proto::Trade& trade) override {
        // Update Mini OMS state first
        if (mini_oms_) {
            mini_oms_->on_trade_execution(trade);
        }
        
        // Then notify strategy
        if (strategy_) {
            strategy_->on_trade_execution(trade);
        }
    }
    
    void set_symbol(const std::string& symbol) override {
        if (strategy_) {
            strategy_->set_symbol(symbol);
        }
    }
    
    void set_exchange(const std::string& exchange) override {
        if (strategy_) {
            strategy_->set_exchange(exchange);
        }
    }
    
    const std::string& get_name() const override {
        static const std::string empty_name = "";
        return strategy_ ? strategy_->get_name() : empty_name;
    }
    
    void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) override {
        oms_adapter_ = adapter;
        if (mini_oms_) {
            mini_oms_->set_oms_adapter(adapter);
        }
    }
    
    void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) override {
        mds_adapter_ = adapter;
        if (mini_oms_) {
            mini_oms_->set_mds_adapter(adapter);
        }
    }
    
    void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) override {
        pms_adapter_ = adapter;
        if (mini_oms_) {
            mini_oms_->set_pms_adapter(adapter);
        }
    }
    
    // Order management methods (Strategy calls Container, Container uses Mini OMS)
    bool send_order(const std::string& cl_ord_id,
                   const std::string& symbol,
                   proto::Side side,
                   proto::OrderType type,
                   double qty,
                   double price = 0.0) {
        if (!strategy_ || !strategy_->is_running()) {
            return false;
        }
        
        // Strategy calls Container, Container uses Mini OMS
        return mini_oms_->send_order(cl_ord_id, symbol, side, type, qty, price);
    }
    
    bool cancel_order(const std::string& cl_ord_id) {
        if (!strategy_ || !strategy_->is_running()) {
            return false;
        }
        
        // Strategy calls Container, Container uses Mini OMS
        return mini_oms_->cancel_order(cl_ord_id);
    }
    
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
        if (!strategy_ || !strategy_->is_running()) {
            return false;
        }
        
        // Strategy calls Container, Container uses Mini OMS
        return mini_oms_->modify_order(cl_ord_id, new_price, new_qty);
    }
    
    // Mini OMS access for order state queries
    MiniOMS* get_mini_oms() const {
        return mini_oms_.get();
    }
    
    // Additional methods
    bool has_strategy() const {
        return strategy_ != nullptr;
    }
    
    std::shared_ptr<AbstractStrategy> get_strategy() const {
        return strategy_;
    }

private:
    std::shared_ptr<AbstractStrategy> strategy_;
    std::shared_ptr<ZmqOMSAdapter> oms_adapter_;
    std::shared_ptr<ZmqMDSAdapter> mds_adapter_;
    std::shared_ptr<ZmqPMSAdapter> pms_adapter_;
    std::unique_ptr<MiniOMS> mini_oms_;
};
