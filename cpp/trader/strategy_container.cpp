#include "strategy_container.hpp"
#include "../strategies/base_strategy/abstract_strategy.hpp"
#include <iostream>

/**
 * Strategy Container Implementation
 * 
 * Holds a single strategy instance and delegates all events to it.
 * Uses Mini OMS for order state management and ZMQ adapter routing.
 */

StrategyContainer::StrategyContainer() {
    mini_oms_ = std::make_unique<MiniOMS>();
    mini_pms_ = std::make_unique<trader::MiniPMS>();
}

StrategyContainer::~StrategyContainer() = default;

// Set the strategy instance
void StrategyContainer::set_strategy(std::shared_ptr<AbstractStrategy> strategy) {
    strategy_ = strategy;
    // Strategy doesn't know about adapters - it delegates to container
}

// IStrategyContainer interface implementation
void StrategyContainer::start() {
    if (mini_oms_) {
        mini_oms_->start();
    }
    if (mini_pms_) {
        mini_pms_->start();
    }
    running_.store(true);
    std::cout << "[STRATEGY_CONTAINER] Started" << std::endl;
}

void StrategyContainer::stop() {
    running_.store(false);
    if (mini_oms_) {
        mini_oms_->stop();
    }
    if (mini_pms_) {
        mini_pms_->stop();
    }
    std::cout << "[STRATEGY_CONTAINER] Stopped" << std::endl;
}

bool StrategyContainer::is_running() const {
    return running_.load();
}

// Event handlers - delegate to strategy
void StrategyContainer::on_market_data(const proto::OrderBookSnapshot& orderbook) {
    if (strategy_) {
        strategy_->on_market_data(orderbook);
    }
}

void StrategyContainer::on_order_event(const proto::OrderEvent& order_event) {
    if (strategy_) {
        strategy_->on_order_event(order_event);
    }
}

void StrategyContainer::on_position_update(const proto::PositionUpdate& position) {
    if (strategy_) {
        strategy_->on_position_update(position);
    }
}

void StrategyContainer::on_trade_execution(const proto::Trade& trade) {
    if (strategy_) {
        strategy_->on_trade_execution(trade);
    }
}

void StrategyContainer::on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) {
    if (strategy_) {
        strategy_->on_account_balance_update(balance_update);
    }
}

// Configuration
void StrategyContainer::set_symbol(const std::string& symbol) {
    symbol_ = symbol;
    if (strategy_) {
        strategy_->set_symbol(symbol);
    }
}

void StrategyContainer::set_exchange(const std::string& exchange) {
    exchange_ = exchange;
    if (strategy_) {
        strategy_->set_exchange(exchange);
    }
}

const std::string& StrategyContainer::get_name() const {
    if (strategy_) {
        return strategy_->get_name();
    }
    return name_;
}

// ZMQ adapter setup
void StrategyContainer::set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) {
    oms_adapter_ = adapter;
}

void StrategyContainer::set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) {
    mds_adapter_ = adapter;
}

void StrategyContainer::set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) {
    pms_adapter_ = adapter;
}

// Position queries - delegate to Mini PMS
std::optional<trader::PositionInfo> StrategyContainer::get_position(const std::string& exchange, const std::string& symbol) const {
    if (mini_pms_) {
        return mini_pms_->get_position(exchange, symbol);
    }
    return std::nullopt;
}

std::vector<trader::PositionInfo> StrategyContainer::get_all_positions() const {
    if (mini_pms_) {
        return mini_pms_->get_all_positions();
    }
    return {};
}

std::vector<trader::PositionInfo> StrategyContainer::get_positions_by_exchange(const std::string& exchange) const {
    if (mini_pms_) {
        return mini_pms_->get_positions_by_exchange(exchange);
    }
    return {};
}

std::vector<trader::PositionInfo> StrategyContainer::get_positions_by_symbol(const std::string& symbol) const {
    if (mini_pms_) {
        return mini_pms_->get_positions_by_symbol(symbol);
    }
    return {};
}

// Account balance queries - delegate to Mini PMS
std::optional<trader::AccountBalanceInfo> StrategyContainer::get_account_balance(const std::string& exchange, const std::string& instrument) const {
    if (mini_pms_) {
        return mini_pms_->get_account_balance(exchange, instrument);
    }
    return std::nullopt;
}

std::vector<trader::AccountBalanceInfo> StrategyContainer::get_all_account_balances() const {
    if (mini_pms_) {
        return mini_pms_->get_all_account_balances();
    }
    return {};
}

std::vector<trader::AccountBalanceInfo> StrategyContainer::get_account_balances_by_exchange(const std::string& exchange) const {
    if (mini_pms_) {
        return mini_pms_->get_account_balances_by_exchange(exchange);
    }
    return {};
}

std::vector<trader::AccountBalanceInfo> StrategyContainer::get_account_balances_by_instrument(const std::string& instrument) const {
    if (mini_pms_) {
        return mini_pms_->get_account_balances_by_instrument(instrument);
    }
    return {};
}