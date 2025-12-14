#include "strategy_container.hpp"
#include "../strategies/base_strategy/abstract_strategy.hpp"
#include "../strategies/mm_strategy/market_making_strategy.hpp"
#include "../utils/logging/log_helper.hpp"
#include "../utils/constants.hpp"
#include <thread>
#include <chrono>

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

StrategyContainer::~StrategyContainer() {
    // Signal destruction to prevent use-after-free in timeout thread
    destroyed_.store(true);
    
    // Wait for timeout thread to complete (with timeout)
    if (order_state_timeout_thread_ && order_state_timeout_thread_->joinable()) {
        // Give thread a moment to check destroyed_ flag
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (order_state_timeout_thread_->joinable()) {
            order_state_timeout_thread_->join();
        }
    }
}

// Set the strategy instance
void StrategyContainer::set_strategy(std::shared_ptr<AbstractStrategy> strategy) {
    strategy_ = strategy;
    // Strategy doesn't know about adapters - it delegates to container
    
    // Set order placement callbacks so strategy can send orders
    if (strategy_) {
        strategy_->set_order_sender([this](const std::string& cl_ord_id,
                                          const std::string& symbol,
                                          proto::Side side,
                                          proto::OrderType type,
                                          double qty,
                                          double price) -> bool {
            return this->send_order(cl_ord_id, symbol, side, type, qty, price);
        });
        
        strategy_->set_order_canceller([this](const std::string& cl_ord_id) -> bool {
            return this->cancel_order(cl_ord_id);
        });
        
        strategy_->set_order_modifier([this](const std::string& cl_ord_id,
                                            double new_price,
                                            double new_qty) -> bool {
            return this->modify_order(cl_ord_id, new_price, new_qty);
        });
    }
}

// IStrategyContainer interface implementation
void StrategyContainer::start() {
    logging::Logger logger("STRATEGY_CONTAINER");
    
    // Start MiniOMS and MiniPMS immediately (they need to be running to receive updates)
    if (mini_oms_) {
        mini_oms_->start();
    }
    if (mini_pms_) {
        mini_pms_->start();
    }
    
    // Mark that start was requested, but don't start strategy yet
    strategy_start_requested_.store(true);
    running_.store(true);
    
    logger.info("Container started - waiting for balance, position, and order state before starting strategy");
    
    // Note: We're checking local MiniOMS state, but this will be empty at startup
    // The trading engine should query the exchange for open orders at startup and send them as order events
    // Once we receive order events (or confirm there are none), we mark order state as queried
    // For now, we wait for order events to arrive via ZMQ from the trading engine
    // TODO: Add direct query mechanism to trading engine or exchange for current open orders
    
    // Check local state (will be empty at startup unless orders were sent in this session)
    if (mini_oms_) {
        auto active_orders = mini_oms_->get_active_orders();
        logger.info("Checked local order state - found " + std::to_string(active_orders.size()) + " active orders locally");
        
        // If we have local orders, we've at least checked the state
        // Otherwise, we'll wait for order events from trading engine to sync state
        if (active_orders.size() > 0) {
            order_state_queried_.store(true);
            logger.info("Local orders found - order state considered queried");
        } else {
            // Wait for order events to arrive - trading engine should query exchange at startup
            // and send order events. We'll mark as queried after receiving first order event
            // or after a timeout if no orders exist
            logger.info("No local orders - waiting for order events from trading engine to sync state");
            // Don't mark as queried yet - wait for order events
        }
    } else {
        logger.warn("MiniOMS not available - will wait for order events");
    }
    
    // Check if we can start strategy now (might already have balance/position)
    // Note: order_state_queried might still be false if no local orders exist
    // It will be set to true when we receive order events from trading engine
    check_and_start_strategy();
    
    // If we still haven't queried order state and have no local orders,
    // set a timeout to mark as queried (assuming no orders exist)
    // This prevents indefinite waiting if trading engine doesn't send events
    // Also set a timeout for balance and position if they don't arrive
    if (!order_state_queried_.load() && mini_oms_) {
        // Start a background thread to timeout after a few seconds
        // If no order events arrive, assume there are no open orders
        // Store thread as member to ensure proper cleanup
        order_state_timeout_thread_.reset(new std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(constants::timeout::ORDER_STATE_TIMEOUT_SECONDS));
            // Check if object was destroyed before accessing members
            if (!destroyed_.load() && !order_state_queried_.load()) {
                logging::Logger logger("STRATEGY_CONTAINER");
                logger.info("Timeout waiting for order events - assuming no open orders exist");
                order_state_queried_.store(true);
                check_and_start_strategy();
            }
        }));
    }
    
    // Set timeout for balance and position if they don't arrive
    // This prevents strategy from never starting if exchange doesn't send updates
    if ((!balance_received_.load() || !position_received_.load()) && strategy_) {
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(constants::timeout::BALANCE_POSITION_TIMEOUT_SECONDS));
            if (!destroyed_.load() && strategy_start_requested_.load() && !strategy_fully_started_.load()) {
                logging::Logger logger("STRATEGY_CONTAINER");
                bool balance_ok = balance_received_.load();
                bool position_ok = position_received_.load();
                
                if (!balance_ok) {
                    logger.warn("Timeout waiting for balance update - proceeding without balance");
                    balance_received_.store(true);  // Mark as received to allow startup
                }
                if (!position_ok) {
                    logger.warn("Timeout waiting for position update - proceeding without position");
                    position_received_.store(true);  // Mark as received to allow startup
                }
                
                check_and_start_strategy();
            }
        }).detach();  // This timeout thread is short-lived and safe to detach
    }
}

void StrategyContainer::stop() {
    running_.store(false);
    if (mini_oms_) {
        mini_oms_->stop();
    }
    if (mini_pms_) {
        mini_pms_->stop();
    }
    LOG_INFO_COMP("STRATEGY_CONTAINER", "Stopped");
}

bool StrategyContainer::is_running() const {
    return running_.load();
}

// Event handlers - delegate to strategy
void StrategyContainer::on_market_data(const proto::OrderBookSnapshot& orderbook) {
    // Only forward market data if strategy is fully started
    if (strategy_ && strategy_fully_started_.load()) {
        strategy_->on_market_data(orderbook);
    }
}

void StrategyContainer::on_order_event(const proto::OrderEvent& order_event) {
    logging::Logger logger("STRATEGY_CONTAINER");
    
    // Update MiniOMS first
    if (mini_oms_) {
        mini_oms_->on_order_event(order_event);
        
        // If we haven't marked order state as queried yet, do so now
        // This means the trading engine has synced order state (either by querying or sending events)
        if (!order_state_queried_.load()) {
            order_state_queried_.store(true);
            logger.info("Received first order event - order state synced from trading engine");
            // Check if we can start strategy now
            check_and_start_strategy();
        }
    }
    
    // Only forward order events if strategy is fully started
    if (strategy_ && strategy_fully_started_.load()) {
        strategy_->on_order_event(order_event);
    }
}

void StrategyContainer::on_position_update(const proto::PositionUpdate& position) {
    logging::Logger logger("STRATEGY_CONTAINER");
    
    // Mark that we've received at least one position update
    if (!position_received_.load()) {
        position_received_.store(true);
        logger.info("Received first position update - " + position.symbol() + 
                    " qty: " + std::to_string(position.qty()));
    }
    
    // Update MiniPMS
    if (mini_pms_) {
        mini_pms_->update_position(position);
    }
    
    // Forward to strategy only if it's fully started
    if (strategy_ && strategy_fully_started_.load()) {
        strategy_->on_position_update(position);
    }
    
    // Check if we can start strategy now
    check_and_start_strategy();
}

void StrategyContainer::on_trade_execution(const proto::Trade& trade) {
    // Only forward trade executions if strategy is fully started
    if (strategy_ && strategy_fully_started_.load()) {
        strategy_->on_trade_execution(trade);
    }
}

void StrategyContainer::on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) {
    logging::Logger logger("STRATEGY_CONTAINER");
    
    // Mark that we've received balance update
    if (!balance_received_.load()) {
        balance_received_.store(true);
        logger.info("Received first balance update - " + std::to_string(balance_update.balances_size()) + " balances");
    }
    
    // Update MiniPMS
    if (mini_pms_) {
        mini_pms_->update_account_balance(balance_update);
    }
    
    // Forward to strategy only if it's fully started
    if (strategy_ && strategy_fully_started_.load()) {
        strategy_->on_account_balance_update(balance_update);
    }
    
    // Check if we can start strategy now
    check_and_start_strategy();
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
    // Set exchange name on MiniOMS so it can include it in orders
    if (mini_oms_) {
        mini_oms_->set_exchange_name(exchange);
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

// Order placement - delegate to MiniOMS
bool StrategyContainer::send_order(const std::string& cl_ord_id,
                                  const std::string& symbol,
                                  proto::Side side,
                                  proto::OrderType type,
                                  double qty,
                                  double price) {
    if (mini_oms_) {
        return mini_oms_->send_order(cl_ord_id, symbol, side, type, qty, price);
    }
    return false;
}

bool StrategyContainer::cancel_order(const std::string& cl_ord_id) {
    if (mini_oms_) {
        return mini_oms_->cancel_order(cl_ord_id);
    }
    return false;
}

bool StrategyContainer::modify_order(const std::string& cl_ord_id,
                                    double new_price,
                                    double new_qty) {
    if (mini_oms_) {
        return mini_oms_->modify_order(cl_ord_id, new_price, new_qty);
    }
    return false;
}

/**
 * Check if all readiness conditions are met and start strategy if so
 * 
 * Readiness conditions:
 * 1. Balance update received from Position Server
 * 2. Position update received from Position Server
 * 3. Order state queried (either from local state or from Trading Engine)
 * 
 * Once all conditions are met, the strategy is marked as fully started
 * and will receive all future events (market data, order events, positions, balances).
 * 
 * @note This method is called automatically when readiness conditions change.
 * @note If conditions timeout, they may be marked as met to allow partial startup.
 */
void StrategyContainer::check_and_start_strategy() {
    if (!strategy_ || !strategy_start_requested_.load()) {
        return;  // Strategy not set or start not requested
    }
    
    // Check if strategy is already fully started
    if (strategy_fully_started_.load()) {
        return;  // Already started
    }
    
    // Check all readiness conditions
    bool balance_ok = balance_received_.load();
    bool position_ok = position_received_.load();
    bool orders_ok = order_state_queried_.load();
    
    if (balance_ok && position_ok && orders_ok) {
        logging::Logger logger("STRATEGY_CONTAINER");
        logger.info("All readiness conditions met - strategy is now active");
        logger.info("  Balance received: YES");
        logger.info("  Position received: YES");
        logger.info("  Order state queried: YES");
        
        // Strategy is now considered fully started - it will receive all future events
        strategy_fully_started_.store(true);
        LOG_INFO_COMP("STRATEGY_CONTAINER", "Strategy is now active and will receive all events");
    } else {
        logging::Logger logger("STRATEGY_CONTAINER");
        logger.debug("Waiting for readiness conditions:");
        logger.debug("  Balance received: " + std::string(balance_ok ? "YES" : "NO"));
        logger.debug("  Position received: " + std::string(position_ok ? "YES" : "NO"));
        logger.debug("  Order state queried: " + std::string(orders_ok ? "YES" : "NO"));
    }
}