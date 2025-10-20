#include "market_making_strategy.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

MarketMakingStrategy::MarketMakingStrategy(const std::string& symbol,
                                          std::shared_ptr<GlftTarget> glft_model)
    : AbstractStrategy("MarketMakingStrategy"), symbol_(symbol), glft_model_(glft_model) {
    statistics_.reset();
}

void MarketMakingStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    std::cout << "[MARKET_MAKING] Starting market making strategy for " << symbol_ << std::endl;
    running_.store(true);
}

void MarketMakingStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[MARKET_MAKING] Stopping market making strategy" << std::endl;
    running_.store(false);
    
    // Note: Order cancellation is handled by Mini OMS
}

void MarketMakingStrategy::on_market_data(const proto::OrderBookSnapshot& orderbook) {
    if (!running_.load() || orderbook.symbol() != symbol_) {
        return;
    }
    
    process_orderbook(orderbook);
}

void MarketMakingStrategy::on_order_event(const proto::OrderEvent& order_event) {
    if (!running_.load()) {
        return;
    }
    
    std::string cl_ord_id = order_event.cl_ord_id();
    
    // Update statistics based on event
    switch (order_event.event_type()) {
        case proto::OrderEventType::FILL:
            statistics_.filled_orders.fetch_add(1);
            break;
        case proto::OrderEventType::CANCEL:
            statistics_.cancelled_orders.fetch_add(1);
            break;
        default:
            break;
    }
    
    std::cout << "[MARKET_MAKING] Order " << cl_ord_id << " event: " 
              << order_event.event_type() << std::endl;
}

void MarketMakingStrategy::on_position_update(const proto::PositionUpdate& position) {
    if (!running_.load() || position.symbol() != symbol_) {
        return;
    }
    
    // Update inventory delta based on position
    double new_delta = position.qty();
    current_inventory_delta_.store(new_delta);
    
    std::cout << "[MARKET_MAKING] Position update: " << symbol_ 
              << " qty=" << position.qty() << " delta=" << new_delta << std::endl;
    
    // Trigger quote update based on inventory
    manage_inventory();
}

void MarketMakingStrategy::on_trade_execution(const proto::Trade& trade) {
    if (!running_.load() || trade.symbol() != symbol_) {
        return;
    }
    
    // Update statistics
    double trade_value = trade.qty() * trade.price();
    double current_volume = statistics_.total_volume.load();
    statistics_.total_volume.store(current_volume + trade_value);
    
    std::cout << "[MARKET_MAKING] Trade execution: " << trade.symbol() 
              << " " << trade.qty() << " @ " << trade.price() << std::endl;
}

void MarketMakingStrategy::on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) {
    if (!running_.load()) {
        return;
    }
    std::cout << "[MARKET_MAKING] Account Balance Update: " << balance_update.balances_size() << " balances" << std::endl;
    // Update internal balance tracking or risk management
}

// Order management methods removed - Strategy calls Container instead

OrderStateInfo MarketMakingStrategy::get_order_state(const std::string& cl_ord_id) {
    // Note: Strategy doesn't track orders - Mini OMS does
    // This method is kept for compatibility but should not be used
    OrderStateInfo empty_state;
    empty_state.cl_ord_id = cl_ord_id;
    return empty_state;
}

std::vector<OrderStateInfo> MarketMakingStrategy::get_active_orders() {
    // Note: Strategy doesn't track orders - Mini OMS does
    // This method is kept for compatibility but should not be used
    return std::vector<OrderStateInfo>();
}

std::vector<OrderStateInfo> MarketMakingStrategy::get_all_orders() {
    // Note: Strategy doesn't track orders - Mini OMS does
    // This method is kept for compatibility but should not be used
    return std::vector<OrderStateInfo>();
}

// Position queries (delegated to Mini PMS via Container)
// Note: These methods will be implemented by StrategyContainer
// Strategy doesn't directly access Mini PMS - it goes through Container
std::optional<trader::PositionInfo> MarketMakingStrategy::get_position(const std::string& exchange, 
                                                                     const std::string& symbol) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty optional
    return std::nullopt;
}

std::vector<trader::PositionInfo> MarketMakingStrategy::get_all_positions() const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::PositionInfo>();
}

std::vector<trader::PositionInfo> MarketMakingStrategy::get_positions_by_exchange(const std::string& exchange) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::PositionInfo>();
}

std::vector<trader::PositionInfo> MarketMakingStrategy::get_positions_by_symbol(const std::string& symbol) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::PositionInfo>();
}

// Account balance queries (delegated to Mini PMS via Container)
// Note: These methods will be implemented by StrategyContainer
// Strategy doesn't directly access Mini PMS - it goes through Container
std::optional<trader::AccountBalanceInfo> MarketMakingStrategy::get_account_balance(const std::string& exchange,
                                                                                    const std::string& instrument) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty optional
    return std::nullopt;
}

std::vector<trader::AccountBalanceInfo> MarketMakingStrategy::get_all_account_balances() const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::AccountBalanceInfo>();
}

std::vector<trader::AccountBalanceInfo> MarketMakingStrategy::get_account_balances_by_exchange(const std::string& exchange) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::AccountBalanceInfo>();
}

std::vector<trader::AccountBalanceInfo> MarketMakingStrategy::get_account_balances_by_instrument(const std::string& instrument) const {
    // Note: Strategy doesn't directly access Mini PMS
    // This method should be implemented by StrategyContainer
    // For now, return empty vector
    return std::vector<trader::AccountBalanceInfo>();
}

void MarketMakingStrategy::process_orderbook(const proto::OrderBookSnapshot& orderbook) {
    // Process orderbook data and update quotes
    update_quotes();
}

void MarketMakingStrategy::update_quotes() {
    // Update market making quotes based on current market conditions
    // This would integrate with the GLFT model
    if (glft_model_) {
        // Use GLFT model to determine optimal quotes
        std::cout << "[MARKET_MAKING] Updating quotes using GLFT model" << std::endl;
    }
}

void MarketMakingStrategy::manage_inventory() {
    // Manage inventory risk based on current position
    double inventory_delta = current_inventory_delta_.load();
    
    if (std::abs(inventory_delta) > max_position_size_) {
        std::cout << "[MARKET_MAKING] Inventory risk limit exceeded: " << inventory_delta << std::endl;
        // Adjust quotes or close positions
    }
}

std::string MarketMakingStrategy::generate_order_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "MM_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << dis(gen);
    return oss.str();
}