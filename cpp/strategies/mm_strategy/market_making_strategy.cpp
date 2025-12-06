#include "market_making_strategy.hpp"
#include "../../utils/logging/logger.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <chrono>

namespace {
    // Static logger instance for this module
    logging::Logger& get_logger() {
        static logging::Logger logger("MARKET_MAKING");
        return logger;
    }
}

MarketMakingStrategy::MarketMakingStrategy(const std::string& symbol,
                                          std::shared_ptr<GlftTarget> glft_model)
    : AbstractStrategy("MarketMakingStrategy"), symbol_(symbol), glft_model_(glft_model) {
    statistics_.reset();
    last_quote_update_time_ = std::chrono::system_clock::now() - std::chrono::seconds(10); // Initialize to allow first update
}

MarketMakingStrategy::MarketMakingStrategy(const std::string& symbol,
                                          const MarketMakingStrategyConfig& config)
    : AbstractStrategy("MarketMakingStrategy"), symbol_(symbol) {
    statistics_.reset();
    last_quote_update_time_ = std::chrono::system_clock::now() - std::chrono::seconds(10);
    
    // Create GLFT model from config
    GlftTarget::Config glft_config;
    glft_config.risk_aversion = config.glft.risk_aversion;
    glft_config.target_inventory_ratio = config.glft.target_inventory_ratio;
    glft_config.base_spread = config.glft.base_spread;
    glft_config.execution_cost = config.glft.execution_cost;
    glft_config.inventory_penalty = config.glft.inventory_penalty;
    glft_config.terminal_inventory_penalty = config.glft.terminal_inventory_penalty;
    glft_config.max_position_size = config.glft.max_position_size;
    glft_config.inventory_constraint_active = config.glft.inventory_constraint_active;
    
    glft_model_ = std::make_shared<GlftTarget>(glft_config);
    
    // Apply rest of config
    apply_config(config);
}

void MarketMakingStrategy::load_config(const config::ProcessConfigManager& config_manager,
                                      const std::string& section) {
    MarketMakingStrategyConfig config;
    config.load_from_config(config_manager, section);
    apply_config(config);
}

bool MarketMakingStrategy::load_config_from_file(const std::string& config_file,
                                                 const std::string& section) {
    MarketMakingStrategyConfig config;
    if (!config.load_from_file(config_file, section)) {
        return false;
    }
    apply_config(config);
    return true;
}

void MarketMakingStrategy::apply_config(const MarketMakingStrategyConfig& config) {
    // Apply GLFT config (if model exists)
    if (glft_model_) {
        glft_model_->set_risk_aversion(config.glft.risk_aversion);
        glft_model_->set_target_inventory_ratio(config.glft.target_inventory_ratio);
        glft_model_->set_base_spread(config.glft.base_spread);
        glft_model_->set_execution_cost(config.glft.execution_cost);
        glft_model_->set_inventory_penalty(config.glft.inventory_penalty);
        glft_model_->set_max_position_size(config.glft.max_position_size);
    }
    
    // Apply quote sizing parameters
    set_leverage(config.leverage);
    set_base_quote_size_pct(config.base_quote_size_pct);
    set_min_quote_size_pct(config.min_quote_size_pct);
    set_max_quote_size_pct(config.max_quote_size_pct);
    
    // Apply quote update throttling
    set_min_price_change_bps(config.min_price_change_bps);
    set_min_inventory_change_pct(config.min_inventory_change_pct);
    set_quote_update_interval_ms(config.quote_update_interval_ms);
    set_min_quote_price_change_bps(config.min_quote_price_change_bps);
    
    // Apply risk management
    set_min_spread_bps(config.min_spread_bps);
    set_max_position_size(config.max_position_size);
    
    // Apply volatility config
    set_ewma_decay_factor(config.ewma_decay_factor);
}

void MarketMakingStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    get_logger().info("Starting market making strategy for " + symbol_);
    running_.store(true);
}

void MarketMakingStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    get_logger().info("Stopping market making strategy");
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
    
    get_logger().info("Order " + cl_ord_id + " event: " + std::to_string(static_cast<int>(order_event.event_type())));
}

void MarketMakingStrategy::on_position_update(const proto::PositionUpdate& position) {
    if (!running_.load() || position.symbol() != symbol_) {
        return;
    }
    
    // Update inventory delta based on CeFi position
    // Note: This is CeFi-only. Combined inventory is calculated when needed.
    double new_delta = position.qty();
    current_inventory_delta_.store(new_delta);
    
    std::stringstream ss;
    ss << "Position update (CeFi): " << symbol_ << " qty=" << position.qty() << " delta=" << new_delta;
    get_logger().info(ss.str());
    
    // Check if inventory change is significant enough to trigger quote update
    double old_delta = current_inventory_delta_.load();
    double inventory_change_pct = 0.0;
    if (std::abs(old_delta) > 0.0) {
        inventory_change_pct = std::abs((new_delta - old_delta) / old_delta) * 100.0;
    } else if (std::abs(new_delta) > 0.0) {
        inventory_change_pct = 100.0; // New position
    }
    
    // Update quotes if inventory changed significantly
    if (inventory_change_pct >= min_inventory_change_pct_) {
        double spot_price = current_spot_price_.load();
        if (spot_price > 0.0) {
            update_quotes();
        }
    }
    
    // Trigger inventory risk management
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
    
    std::stringstream ss;
    ss << "Trade execution: " << trade.symbol() << " " << trade.qty() << " @ " << trade.price();
    get_logger().info(ss.str());
}

void MarketMakingStrategy::on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) {
    if (!running_.load()) {
        return;
    }
    get_logger().info("Account Balance Update: " + std::to_string(balance_update.balances_size()) + " balances");
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
    // Update spot price from orderbook
    update_spot_price_from_orderbook(orderbook);
    
    // Update EWMA volatility using current price
    double spot_price = current_spot_price_.load();
    if (spot_price > 0.0) {
        update_ewma_volatility(spot_price);
    }
    
    // Check if we should update quotes (throttled)
    if (should_update_quotes(spot_price)) {
        update_quotes();
    }
}

void MarketMakingStrategy::update_quotes() {
    // Update market making quotes based on current market conditions using GLFT model
    if (!glft_model_ || current_spot_price_.load() <= 0.0) {
        return;
    }
    
    // Calculate combined inventory (CeFi + DeFi)
    double spot_price = current_spot_price_.load();
    CombinedInventory combined = calculate_combined_inventory(spot_price);
    
    // Get current volatility
    double volatility = current_volatility_.load();
    
    // Calculate target inventory using GLFT model
    double target_offset = glft_model_->compute_target(
        combined.token0_total,
        combined.token1_total,
        spot_price,
        volatility
    );
    
    std::stringstream ss;
    ss << "GLFT target calculation:" << std::endl
       << "  Combined inventory - Token0: " << combined.token0_total 
       << " (CeFi: " << combined.token0_cefi << ", DeFi: " << combined.token0_defi << ")" << std::endl
       << "  Combined inventory - Token1: " << combined.token1_total 
       << " (CeFi: " << combined.token1_cefi << ", DeFi: " << combined.token1_defi << ")" << std::endl
       << "  Spot price: " << spot_price << std::endl
       << "  Volatility: " << volatility << std::endl
       << "  Target offset: " << target_offset;
    get_logger().debug(ss.str());
    
    // Update current inventory delta with target offset
    current_inventory_delta_.store(target_offset);
    
    // Calculate bid/ask prices based on GLFT model
    // The GLFT model gives us a target offset, but we need to convert this to actual bid/ask prices
    // For market making, we place limit orders around the mid price with a spread
    
    // Calculate effective spread from GLFT components
    double normalized_skew = (combined.token1_total * spot_price) / std::max(combined.token0_total, 1.0);
    double base_spread = glft_model_->get_config().base_spread;
    double risk_aversion = glft_model_->get_config().risk_aversion;
    double inventory_penalty = glft_model_->get_config().inventory_penalty;
    double terminal_penalty = glft_model_->get_config().terminal_inventory_penalty;
    
    // Calculate spread components (matching GLFT logic)
    double inventory_risk = risk_aversion * volatility * volatility * std::abs(normalized_skew) + 
                           inventory_penalty * std::abs(normalized_skew);
    double terminal_risk = terminal_penalty * (normalized_skew * normalized_skew);
    double total_spread = base_spread + inventory_risk + terminal_risk;
    
    // Calculate bid/ask prices
    // Mid price is the current spot price
    double mid_price = spot_price;
    double half_spread = total_spread * mid_price / 2.0;  // Half spread in price units
    
    // Adjust quotes based on target offset
    // Negative target_offset means we want to reduce position -> widen ask (discourage buying), narrow bid (encourage selling)
    // Positive target_offset means we want to increase position -> narrow ask (encourage buying), widen bid (discourage selling)
    double offset_adjustment = target_offset * spot_price / std::max(combined.token0_total, 1.0);  // Normalize adjustment
    
    double bid_price = mid_price - half_spread - offset_adjustment;  // Adjust bid based on target
    double ask_price = mid_price + half_spread - offset_adjustment;  // Adjust ask based on target
    
    // CRITICAL: Ensure quotes never cross the best bid/ask
    // Strategy: If GLFT calculates aggressive quotes that would cross, match best bid/ask on our side
    // This keeps us passive (not taking liquidity) while respecting GLFT's intent to be closer to market
    {
        std::lock_guard<std::mutex> lock(orderbook_mutex_);
        if (best_bid_ > 0.0 && best_ask_ > 0.0) {
            // If bid would cross best ask, set it to best bid (stay passive on bid side)
            // GLFT wants to buy aggressively, but we match best bid to stay passive
            if (bid_price >= best_ask_) {
                std::stringstream ss;
                ss << "Calculated bid (" << bid_price << ") would cross best ask (" << best_ask_ 
                   << "). Setting to best bid (" << best_bid_ << ") to stay passive.";
                get_logger().warn(ss.str());
                bid_price = best_bid_; // Match best bid (passive)
            }
            
            // If ask would cross best bid, set it to best ask (stay passive on ask side)
            // GLFT wants to sell aggressively, but we match best ask to stay passive
            if (ask_price <= best_bid_) {
                std::stringstream ss;
                ss << "Calculated ask (" << ask_price << ") would cross best bid (" << best_bid_ 
                   << "). Setting to best ask (" << best_ask_ << ") to stay passive.";
                get_logger().warn(ss.str());
                ask_price = best_ask_; // Match best ask (passive)
            }
            
            // Ensure bid < ask after adjustments
            if (bid_price >= ask_price) {
                get_logger().error("After anti-cross adjustments, bid >= ask. Skipping quote update to avoid invalid order.");
                return;
            }
        }
    }
    
    // Ensure prices are valid
    if (bid_price > 0.0 && ask_price > bid_price) {
        // Check if quotes actually need to change (avoid unnecessary flickering)
        bool quotes_changed = false;
        {
            std::lock_guard<std::mutex> lock(quote_update_mutex_);
            
            // Check if bid/ask prices changed significantly
            if (last_quote_bid_price_ > 0.0 && last_quote_ask_price_ > 0.0) {
                double bid_change = std::abs(bid_price - last_quote_bid_price_);
                double ask_change = std::abs(ask_price - last_quote_ask_price_);
                double bid_change_bps = (bid_change / last_quote_bid_price_) * 10000.0;
                double ask_change_bps = (ask_change / last_quote_ask_price_) * 10000.0;
                
                // Only update if bid or ask changed by minimum threshold
                if (bid_change_bps >= min_quote_price_change_bps_ || ask_change_bps >= min_quote_price_change_bps_) {
                    quotes_changed = true;
                }
            } else {
                // First quote placement - always update
                quotes_changed = true;
            }
        }
        
        // Only update quotes if they actually changed
        if (!quotes_changed) {
            get_logger().debug("Quotes unchanged, skipping update (bid/ask change < " + 
                              std::to_string(min_quote_price_change_bps_) + " bps)");
            return;
        }
        
        // Calculate dynamic quote sizes based on inventory skew and balance
        // Sizes are calculated as percentages of leveraged collateral balance
        double actual_collateral_balance = std::max(combined.token0_total, 1.0); // Actual collateral
        double leveraged_balance = actual_collateral_balance * leverage_; // Apply leverage multiplier
        
        // Calculate base sizes as percentages of leveraged balance
        double base_size_absolute = leveraged_balance * base_quote_size_pct_;
        double min_size_absolute = leveraged_balance * min_quote_size_pct_;
        double max_size_absolute = leveraged_balance * max_quote_size_pct_;
        
        // When inventory is skewed, quote more on the side that reduces inventory
        // Normalize skew by actual collateral (not leveraged) for risk calculations
        double normalized_skew = (combined.token1_total * spot_price) / actual_collateral_balance;
        
        // Base sizes (symmetric when inventory is neutral)
        double base_bid_size = base_size_absolute;
        double base_ask_size = base_size_absolute;
        
        // Adjust sizes based on inventory skew
        // Positive skew (long position) -> quote more on ask side (to sell)
        // Negative skew (short position) -> quote more on bid side (to buy)
        double skew_factor = std::clamp(std::abs(normalized_skew) * 2.0, 0.0, 1.0); // Scale skew to [0, 1]
        
        if (normalized_skew > 0.0) {
            // Long position: increase ask size, decrease bid size
            base_ask_size = base_size_absolute * (1.0 + skew_factor);
            base_bid_size = base_size_absolute * (1.0 - skew_factor * 0.5); // Reduce bid less aggressively
        } else if (normalized_skew < 0.0) {
            // Short position: increase bid size, decrease ask size
            base_bid_size = base_size_absolute * (1.0 + std::abs(skew_factor));
            base_ask_size = base_size_absolute * (1.0 - std::abs(skew_factor) * 0.5); // Reduce ask less aggressively
        }
        
        // Clamp to max bound, but check min bound - if below minimum, skip that side entirely
        double bid_size = std::clamp(base_bid_size, 0.0, max_size_absolute);
        double ask_size = std::clamp(base_ask_size, 0.0, max_size_absolute);
        
        // Determine which sides to quote (skip if below minimum)
        bool quote_bid = bid_size >= min_size_absolute;
        bool quote_ask = ask_size >= min_size_absolute;
        
        // If both sides are below minimum, skip quote update entirely
        if (!quote_bid && !quote_ask) {
            get_logger().warn("Both bid and ask sizes below minimum (" + std::to_string(min_size_absolute) + 
                             "). Skipping quote update.");
            return;
        }
        
        std::stringstream ss;
        ss << "Calculated quotes:" << std::endl
           << "  Mid price: " << mid_price << std::endl
           << "  Total spread: " << (total_spread * 10000) << " bps" << std::endl
           << "  Actual collateral: " << actual_collateral_balance << std::endl
           << "  Leverage: " << leverage_ << "x" << std::endl
           << "  Leveraged balance: " << leveraged_balance << " (used for sizing)" << std::endl
           << "  Bid price: " << bid_price << " | Bid size: " << bid_size;
        if (!quote_bid) {
            ss << " (SKIPPED - below min " << min_size_absolute << ")";
        }
        ss << std::endl << "  Ask price: " << ask_price << " | Ask size: " << ask_size;
        if (!quote_ask) {
            ss << " (SKIPPED - below min " << min_size_absolute << ")";
        }
        ss << std::endl << "  Inventory skew: " << normalized_skew << " (affects size asymmetry)" << std::endl
           << "  Min size threshold: " << min_size_absolute << " (" << (min_quote_size_pct_ * 100) << "% of leveraged balance)";
        get_logger().debug(ss.str());
        
        // Cancel existing orders before placing new ones
        {
            std::lock_guard<std::mutex> lock(quote_update_mutex_);
            for (const auto& order_id : active_order_ids_) {
                cancel_order(order_id);
            }
            active_order_ids_.clear();
        }
        
        // Place bid order (buy limit order) only if size is above minimum
        std::string bid_order_id;
        if (quote_bid) {
            bid_order_id = generate_order_id() + "_BID";
            if (send_order(bid_order_id, symbol_, proto::BUY, proto::LIMIT, bid_size, bid_price)) {
                std::stringstream ss;
                ss << "Placed bid order: " << bid_order_id << " " << bid_size << " @ " << bid_price;
                get_logger().info(ss.str());
                std::lock_guard<std::mutex> lock(quote_update_mutex_);
                active_order_ids_.push_back(bid_order_id);
            } else {
                get_logger().error("Failed to place bid order");
            }
        } else {
            get_logger().debug("Skipping bid order - size (" + std::to_string(bid_size) + 
                              ") below minimum (" + std::to_string(min_size_absolute) + ")");
        }
        
        // Place ask order (sell limit order) only if size is above minimum
        std::string ask_order_id;
        if (quote_ask) {
            ask_order_id = generate_order_id() + "_ASK";
            if (send_order(ask_order_id, symbol_, proto::SELL, proto::LIMIT, ask_size, ask_price)) {
                std::stringstream ss;
                ss << "Placed ask order: " << ask_order_id << " " << ask_size << " @ " << ask_price;
                get_logger().info(ss.str());
                std::lock_guard<std::mutex> lock(quote_update_mutex_);
                active_order_ids_.push_back(ask_order_id);
            } else {
                get_logger().error("Failed to place ask order");
            }
        } else {
            get_logger().debug("Skipping ask order - size (" + std::to_string(ask_size) + 
                              ") below minimum (" + std::to_string(min_size_absolute) + ")");
        }
        
        // Track active order IDs and prices for cancellation on next update
        {
            std::lock_guard<std::mutex> lock(quote_update_mutex_);
            // Only update prices for sides that were actually quoted
            if (quote_bid) {
                last_quote_bid_price_ = bid_price;
            }
            if (quote_ask) {
                last_quote_ask_price_ = ask_price;
            }
            last_mid_price_ = mid_price;
            last_quote_update_time_ = std::chrono::system_clock::now();
        }
    }
}

bool MarketMakingStrategy::should_update_quotes(double current_mid_price) const {
    std::lock_guard<std::mutex> lock(quote_update_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_quote_update_time_).count();
    
    // Always update if enough time has passed (time-based refresh)
    if (time_since_last_update >= quote_update_interval_ms_) {
        return true;
    }
    
    // Update if price moved significantly (price movement threshold)
    if (last_mid_price_ > 0.0) {
        double price_change = std::abs(current_mid_price - last_mid_price_);
        double price_change_bps = (price_change / last_mid_price_) * 10000.0;
        if (price_change_bps >= min_price_change_bps_) {
            return true;
        }
    }
    
    // Update if inventory changed significantly (checked in on_position_update)
    // This is handled separately via manage_inventory() -> update_quotes()
    
    return false;
}

void MarketMakingStrategy::manage_inventory() {
    // Manage inventory risk based on combined inventory
    double spot_price = current_spot_price_.load();
    if (spot_price <= 0.0) {
        return;
    }
    
    CombinedInventory combined = calculate_combined_inventory(spot_price);
    
    // Calculate total position value
    double total_value = combined.token0_total + (combined.token1_total / spot_price);
    
    // Check if inventory exceeds limits
    double inventory_ratio_0 = combined.token0_total / total_value;
    double inventory_ratio_1 = (combined.token1_total / spot_price) / total_value;
    
    if (inventory_ratio_0 > max_position_size_ / 100.0 || inventory_ratio_1 > max_position_size_ / 100.0) {
        std::stringstream ss;
        ss << "Inventory risk limit exceeded:" << std::endl
           << "  Token0 ratio: " << inventory_ratio_0 << std::endl
           << "  Token1 ratio: " << inventory_ratio_1;
        get_logger().warn(ss.str());
        // Adjust quotes or close positions
    }
}

// DeFi position management
void MarketMakingStrategy::update_defi_position(const DefiPosition& position) {
    std::lock_guard<std::mutex> lock(defi_positions_mutex_);
    defi_positions_[position.pool_address] = position;
    
    std::stringstream ss;
    ss << "Updated DeFi position: " << position.pool_address
       << " Token0: " << position.token0_amount
       << " Token1: " << position.token1_amount;
    get_logger().info(ss.str());
}

void MarketMakingStrategy::remove_defi_position(const std::string& pool_address) {
    std::lock_guard<std::mutex> lock(defi_positions_mutex_);
    defi_positions_.erase(pool_address);
    
    get_logger().info("Removed DeFi position: " + pool_address);
}

std::vector<MarketMakingStrategy::DefiPosition> MarketMakingStrategy::get_defi_positions() const {
    std::lock_guard<std::mutex> lock(defi_positions_mutex_);
    std::vector<DefiPosition> positions;
    positions.reserve(defi_positions_.size());
    
    for (const auto& [pool_address, position] : defi_positions_) {
        positions.push_back(position);
    }
    
    return positions;
}

// Combined inventory calculation
MarketMakingStrategy::CombinedInventory MarketMakingStrategy::calculate_combined_inventory(double spot_price) const {
    CombinedInventory inventory;
    
    // Get CeFi positions from exchange via Mini PMS (queried through StrategyContainer)
    // Positions come from exchange API/WebSocket, not from wallet directly
    // Flow: Exchange API -> Position Server -> Mini PMS -> Strategy (via get_position())
    
    // Query positions for the current symbol from the exchange
    // For perpetual futures: position quantity is the inventory we track
    auto position_info = get_position(exchange_, symbol_);
    if (position_info.has_value()) {
        // token1 = perpetual position quantity (e.g., BTC position in BTCUSDT_Perp)
        inventory.token1_cefi = position_info->qty;
    } else {
        // Fallback: use cached inventory delta if position query fails
        inventory.token1_cefi = current_inventory_delta_.load();
    }
    
    // Get account balances for collateral (token0, e.g., USDC)
    // Balances also come from exchange API via Mini PMS
    auto balance_info = get_account_balance(exchange_, "USDT");  // Assuming USDT/USDC as collateral
    if (balance_info.has_value()) {
        inventory.token0_cefi = balance_info->available + balance_info->locked;
    } else {
        // Try alternative collateral symbols
        balance_info = get_account_balance(exchange_, "USDC");
        if (balance_info.has_value()) {
            inventory.token0_cefi = balance_info->available + balance_info->locked;
        }
    }
    
    // DeFi inventory (from Uniswap V3 LP positions)
    // These are tracked separately and updated via update_defi_position()
    // In production, DeFi positions would be queried from blockchain (Uniswap V3 contracts)
    {
        std::lock_guard<std::mutex> lock(defi_positions_mutex_);
        for (const auto& [pool_address, position] : defi_positions_) {
            inventory.token0_defi += position.token0_amount;
            inventory.token1_defi += position.token1_amount;
        }
    }
    
    // Calculate combined totals
    inventory.token0_total = inventory.token0_cefi + inventory.token0_defi;
    inventory.token1_total = inventory.token1_cefi + inventory.token1_defi;
    
    return inventory;
}

double MarketMakingStrategy::calculate_volatility_from_orderbook(const proto::OrderBookSnapshot& orderbook) const {
    // Fallback: Simple volatility estimate from bid-ask spread
    // This is used only if EWMA hasn't been initialized yet
    
    if (orderbook.bids_size() == 0 || orderbook.asks_size() == 0) {
        return current_volatility_.load(); // Return current volatility if no data
    }
    
    double best_bid = orderbook.bids(0).price();
    double best_ask = orderbook.asks(0).price();
    
    if (best_bid <= 0.0 || best_ask <= 0.0) {
        return current_volatility_.load();
    }
    
    double mid_price = (best_bid + best_ask) / 2.0;
    double spread = best_ask - best_bid;
    double spread_ratio = spread / mid_price;
    
    // Convert spread ratio to annualized volatility estimate (fallback method)
    double volatility_estimate = spread_ratio * std::sqrt(252.0); // Annualize assuming daily trading
    
    // Clamp to reasonable range
    volatility_estimate = std::clamp(volatility_estimate, 0.01, 2.0);
    
    return volatility_estimate;
}

void MarketMakingStrategy::update_ewma_volatility(double current_price) {
    if (current_price <= 0.0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(volatility_mutex_);
    
    if (!volatility_initialized_) {
        // Initialize EWMA variance with a default value
        // Use a conservative estimate: 2% daily volatility = 0.02^2 = 0.0004 variance
        ewma_variance_ = 0.0004;  // (0.02)^2
        last_price_ = current_price;
        volatility_initialized_ = true;
        
        // Set initial volatility
        double initial_vol = std::sqrt(ewma_variance_) * std::sqrt(252.0); // Annualized
        current_volatility_.store(initial_vol);
        return;
    }
    
    // Calculate log return: r_t = ln(P_t / P_{t-1})
    double log_return = std::log(current_price / last_price_);
    
    // Update EWMA variance: σ²_t = λ * σ²_{t-1} + (1-λ) * r²_t
    // where λ (lambda) is the decay factor (typically 0.94-0.97 for daily data)
    ewma_variance_ = ewma_decay_factor_ * ewma_variance_ + (1.0 - ewma_decay_factor_) * (log_return * log_return);
    
    // Update last price
    last_price_ = current_price;
    
    // Calculate annualized volatility: σ_annual = σ_daily * √252
    // where σ_daily = √variance
    double daily_volatility = std::sqrt(ewma_variance_);
    double annualized_volatility = daily_volatility * std::sqrt(252.0);
    
    // Clamp to reasonable range (0.1% to 200% annualized)
    annualized_volatility = std::clamp(annualized_volatility, 0.001, 2.0);
    
    // Update current volatility
    current_volatility_.store(annualized_volatility);
}

void MarketMakingStrategy::update_spot_price_from_orderbook(const proto::OrderBookSnapshot& orderbook) {
    if (orderbook.bids_size() == 0 || orderbook.asks_size() == 0) {
        return;
    }
    
    double best_bid = orderbook.bids(0).price();
    double best_ask = orderbook.asks(0).price();
    
    if (best_bid > 0.0 && best_ask > 0.0) {
        double mid_price = (best_bid + best_ask) / 2.0;
        current_spot_price_.store(mid_price);
        
        // Store best bid/ask for quote validation
        {
            std::lock_guard<std::mutex> lock(orderbook_mutex_);
            best_bid_ = best_bid;
            best_ask_ = best_ask;
        }
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

MarketMakingStrategyConfig MarketMakingStrategy::get_config() const {
    MarketMakingStrategyConfig config;
    
    // Get GLFT config
    if (glft_model_) {
        const auto& glft_cfg = glft_model_->get_config();
        config.glft.risk_aversion = glft_cfg.risk_aversion;
        config.glft.target_inventory_ratio = glft_cfg.target_inventory_ratio;
        config.glft.base_spread = glft_cfg.base_spread;
        config.glft.execution_cost = glft_cfg.execution_cost;
        config.glft.inventory_penalty = glft_cfg.inventory_penalty;
        config.glft.terminal_inventory_penalty = glft_cfg.terminal_inventory_penalty;
        config.glft.max_position_size = glft_cfg.max_position_size;
        config.glft.inventory_constraint_active = glft_cfg.inventory_constraint_active;
    }
    
    // Get quote sizing parameters
    config.leverage = leverage_;
    config.base_quote_size_pct = base_quote_size_pct_;
    config.min_quote_size_pct = min_quote_size_pct_;
    config.max_quote_size_pct = max_quote_size_pct_;
    
    // Get quote update throttling
    config.min_price_change_bps = min_price_change_bps_;
    config.min_inventory_change_pct = min_inventory_change_pct_;
    config.quote_update_interval_ms = quote_update_interval_ms_;
    config.min_quote_price_change_bps = min_quote_price_change_bps_;
    
    // Get risk management
    config.min_spread_bps = min_spread_bps_;
    config.max_position_size = max_position_size_;
    
    // Get volatility config
    config.ewma_decay_factor = ewma_decay_factor_;
    
    return config;
}