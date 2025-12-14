#include "market_making_strategy.hpp"
#include "../../utils/logging/logger.hpp"
#include "../../utils/exchange/exchange_symbol_registry.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <chrono>
#include <atomic>

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
    
    // Apply micro price skew config
    micro_price_skew_alpha_ = config.micro_price_skew_alpha;
    
    // Apply net inventory skew config
    net_inventory_skew_gamma_ = config.net_inventory_skew_gamma;
    
    // Apply DeFi inventory flow weights
    flow_5s_weight_ = config.defi_flow_5s_weight;
    flow_1m_weight_ = config.defi_flow_1m_weight;
    flow_5m_weight_ = config.defi_flow_5m_weight;
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
    
    // Calculate CeFi-only inventory (GLFT uses only CeFi inventory)
    double spot_price = current_spot_price_.load();
    CeFiInventory cefi = calculate_cefi_inventory();
    
    // Get current volatility
    double volatility = current_volatility_.load();
    
    // Convert CeFi position from contracts to tokens for GLFT
    // GLFT expects token1 in tokens (BTC), but cefi.token1 is in contracts
    auto& symbol_registry = ExchangeSymbolRegistry::get_instance();
    double cefi_token1_tokens = 0.0;
    if (cefi.token1 != 0.0 && spot_price > 0.0) {
        cefi_token1_tokens = symbol_registry.contracts_to_token_qty(
            exchange_, symbol_, cefi.token1, spot_price);
    }
    
    // Calculate target inventory using GLFT model (CeFi-only)
    // GLFT expects: token0 (collateral in USD), token1 (position in tokens/BTC)
    double target_offset = glft_model_->compute_target(
        cefi.token0,           // Collateral in USD (CeFi only)
        cefi_token1_tokens,   // Position in tokens (BTC), converted from contracts (CeFi only)
        spot_price,
        volatility
    );
    
    std::stringstream ss;
    ss << "GLFT target calculation (CeFi-only):" << std::endl
       << "  CeFi inventory - Token0 (collateral): " << cefi.token0 << " USD" << std::endl
       << "  CeFi inventory - Token1: " << cefi.token1 << " contracts = " << cefi_token1_tokens << " tokens" << std::endl
       << "  Spot price: " << spot_price << std::endl
       << "  Volatility: " << volatility << std::endl
       << "  Target offset: " << target_offset;
    get_logger().debug(ss.str());
    
    // Update current inventory delta with target offset
    current_inventory_delta_.store(target_offset);
    
    // Calculate bid/ask prices based on GLFT model (CeFi-only)
    // The GLFT model gives us a target offset, but we need to convert this to actual bid/ask prices
    // For market making, we place limit orders around the mid price with a spread
    
    // Calculate effective spread from GLFT components
    // Use CeFi token1 in tokens (not contracts) for skew calculation
    double normalized_skew = (cefi_token1_tokens * spot_price) / std::max(cefi.token0, 1.0);
    double base_spread = glft_model_->get_config().base_spread;
    double risk_aversion = glft_model_->get_config().risk_aversion;
    double inventory_penalty = glft_model_->get_config().inventory_penalty;
    double terminal_penalty = glft_model_->get_config().terminal_inventory_penalty;
    
    // Calculate spread components (matching GLFT logic)
    double inventory_risk = risk_aversion * volatility * volatility * std::abs(normalized_skew) + 
                           inventory_penalty * std::abs(normalized_skew);
    double terminal_risk = terminal_penalty * (normalized_skew * normalized_skew);
    double total_spread = base_spread + inventory_risk + terminal_risk;
    
    // Calculate base bid/ask prices from GLFT (CeFi-only)
    // Mid price is the current spot price
    double mid_price = spot_price;
    double half_spread = total_spread * mid_price / 2.0;  // Half spread in price units
    
    // Adjust quotes based on GLFT target offset (CeFi-only)
    // Negative target_offset means we want to reduce position -> widen ask (discourage buying), narrow bid (encourage selling)
    // Positive target_offset means we want to increase position -> narrow ask (encourage buying), widen bid (discourage selling)
    double offset_adjustment = target_offset * spot_price / std::max(cefi.token0, 1.0);  // Normalize adjustment
    
    double bid_price = mid_price - half_spread - offset_adjustment;  // Base bid from GLFT
    double ask_price = mid_price + half_spread - offset_adjustment;  // Base ask from GLFT
    
    // Apply micro price skew signal (order flow imbalance)
    // Widen spread based on micro price deviation from mid and orderbook imbalance
    // This is a risk management measure - wider spread when there's order flow imbalance
    double micro_price_skew = get_micro_price_skew();
    double orderbook_imbalance = get_orderbook_imbalance();
    
    if (std::abs(micro_price_skew) > 0.0001 || std::abs(orderbook_imbalance) > 0.1) {
        // Combine micro price deviation and orderbook imbalance
        // Micro price deviation: |micro_price - mid_price| / mid_price
        // Orderbook imbalance: |bid_qty - ask_qty| / (bid_qty + ask_qty)
        double micro_deviation = std::abs(micro_price_skew);
        double imbalance_magnitude = std::abs(orderbook_imbalance);
        
        // Combined signal: use max of micro price deviation and imbalance
        // Clamp to reasonable range (max 0.5% = 50 bps)
        double combined_signal = std::max(micro_deviation, imbalance_magnitude);
        double clamped_signal = std::clamp(combined_signal, 0.0, 0.005);
        
        // Apply alpha parameter to control sensitivity
        double adjusted_signal = clamped_signal * micro_price_skew_alpha_;
        
        // Widen spread symmetrically: both bid and ask move away from mid
        // This reduces risk when there's order flow imbalance
        double spread_widening = adjusted_signal * mid_price;
        bid_price -= spread_widening;  // Move bid down (away from mid)
        ask_price += spread_widening;  // Move ask up (away from mid)
        
        std::stringstream micro_ss;
        micro_ss << "Applied micro price spread widening: "
                 << "micro_dev=" << (micro_deviation * 10000) << " bps, "
                 << "imbalance=" << (imbalance_magnitude * 100) << "%, "
                 << "combined=" << (combined_signal * 10000) << " bps, "
                 << "alpha=" << micro_price_skew_alpha_ 
                 << ", final widening=" << (adjusted_signal * 10000) << " bps";
        get_logger().debug(micro_ss.str());
    }
    
    // Apply net inventory skew (CeFi inventory + DeFi flow combined)
    // Calculate net inventory: CeFi position + DeFi flow (both in contracts)
    // Normalize as % of collateral and apply single asymmetric skew
    double defi_flow_contracts = cached_defi_flow_contracts_.load();
    double net_inventory_contracts = cefi.token1 + defi_flow_contracts;  // Net position in contracts
    
    if (std::abs(net_inventory_contracts) > 0.0001 && cefi.token0 > 0.0 && spot_price > 0.0) {
        // Convert net inventory from contracts to tokens
        double net_inventory_tokens = symbol_registry.contracts_to_token_qty(
            exchange_, symbol_, net_inventory_contracts, spot_price);
        
        // Normalize net inventory: convert to USD value, then divide by collateral
        // Result is unitless percentage (e.g., 0.1 = 10% of collateral)
        double net_skew_normalized = (net_inventory_tokens * spot_price) / cefi.token0;
        
        // Apply gamma parameter to control sensitivity
        double adjusted_skew_normalized = net_skew_normalized * net_inventory_skew_gamma_;
        
        // Calculate skew factor (max 10% skew)
        double net_skew_factor = std::clamp(std::abs(adjusted_skew_normalized), 0.0, 0.1);
        
        // Calculate skew adjustment in price units
        double skew_adjustment = net_skew_factor * mid_price;
        
        if (net_inventory_contracts > 0.0) {
            // Net long position: encourage selling to bring inventory to zero
            // Bring ask closer (narrow) = ask_price DOWN (toward mid)
            // Widen bid = bid_price DOWN (away from mid)
            bid_price -= skew_adjustment;  // Move bid down (wider, away from mid)
            ask_price -= skew_adjustment;  // Move ask down (closer/narrow, toward mid)
        } else {
            // Net short position: encourage buying to bring inventory to zero
            // Bring bid closer (narrow) = bid_price UP (toward mid)
            // Widen ask = ask_price UP (away from mid)
            bid_price += skew_adjustment;  // Move bid up (closer/narrow, toward mid)
            ask_price += skew_adjustment;  // Move ask up (wider, away from mid)
        }
        
        std::stringstream skew_ss;
        skew_ss << "Applied net inventory skew: "
                << "CeFi=" << cefi.token1 << " contracts, "
                << "DeFi_flow=" << defi_flow_contracts << " contracts, "
                << "Net=" << net_inventory_contracts << " contracts (" 
                << net_inventory_tokens << " tokens), "
                << "normalized: " << (net_skew_normalized * 100) << "% of collateral, "
                << "gamma=" << net_inventory_skew_gamma_ 
                << ", adjusted: " << (adjusted_skew_normalized * 100) << "%, "
                << "skew factor: " << (net_skew_factor * 100) << "%";
        get_logger().debug(skew_ss.str());
    }
    
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
        
        // Calculate dynamic quote sizes based on CeFi inventory skew and balance
        // Sizes are calculated as percentages of leveraged collateral balance
        double actual_collateral_balance = std::max(cefi.token0, 1.0); // CeFi collateral only
        double leveraged_balance = actual_collateral_balance * leverage_; // Apply leverage multiplier
        
        // Calculate base sizes as percentages of leveraged balance
        double base_size_absolute = leveraged_balance * base_quote_size_pct_;
        double min_size_absolute = leveraged_balance * min_quote_size_pct_;
        double max_size_absolute = leveraged_balance * max_quote_size_pct_;
        
        // When CeFi inventory is skewed, quote more on the side that reduces inventory
        // Normalize skew by actual collateral (not leveraged) for risk calculations
        double normalized_skew = (cefi_token1_tokens * spot_price) / actual_collateral_balance;
        
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
        
        // CRITICAL: Round prices and sizes to exchange tick/step sizes BEFORE validation
        // This ensures the strategy knows exactly what prices/sizes will be sent
        auto& symbol_registry = ExchangeSymbolRegistry::get_instance();
        double original_bid_price = bid_price;
        double original_ask_price = ask_price;
        double original_bid_size = bid_size;
        double original_ask_size = ask_size;
        
        // Initialize quote flags - assume both sides are valid initially
        bool quote_bid = true;
        bool quote_ask = true;
        
        // Round prices and sizes to exchange requirements
        if (!symbol_registry.validate_and_round(exchange_, symbol_, bid_size, bid_price)) {
            get_logger().error("Failed to validate/round bid quote - skipping bid order");
            quote_bid = false;
        }
        
        if (!symbol_registry.validate_and_round(exchange_, symbol_, ask_size, ask_price)) {
            get_logger().error("Failed to validate/round ask quote - skipping ask order");
            quote_ask = false;
        }
        
        // After rounding, validate that bid < ask (rounding might cause them to cross)
        if (quote_bid && quote_ask && bid_price >= ask_price) {
            get_logger().warn("After rounding, bid (" + std::to_string(bid_price) + 
                             ") >= ask (" + std::to_string(ask_price) + 
                             "). Skipping quote update to avoid invalid orders.");
            return;
        }
        
        // Re-check minimum size after rounding (rounded size might be below minimum)
        bool quote_bid_after_rounding = quote_bid && bid_size >= min_size_absolute;
        bool quote_ask_after_rounding = quote_ask && ask_size >= min_size_absolute;
        
        // If both sides are below minimum after rounding, skip quote update entirely
        if (!quote_bid_after_rounding && !quote_ask_after_rounding) {
            get_logger().warn("Both bid and ask sizes below minimum after rounding (" + 
                             std::to_string(min_size_absolute) + 
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
           << "  Bid price: " << original_bid_price;
        if (original_bid_price != bid_price) {
            ss << " -> " << bid_price << " (rounded)";
        }
        ss << " | Bid size: " << original_bid_size;
        if (original_bid_size != bid_size) {
            ss << " -> " << bid_size << " (rounded)";
        }
        if (!quote_bid_after_rounding) {
            ss << " (SKIPPED - below min " << min_size_absolute << " after rounding)";
        }
        ss << std::endl << "  Ask price: " << original_ask_price;
        if (original_ask_price != ask_price) {
            ss << " -> " << ask_price << " (rounded)";
        }
        ss << " | Ask size: " << original_ask_size;
        if (original_ask_size != ask_size) {
            ss << " -> " << ask_size << " (rounded)";
        }
        if (!quote_ask_after_rounding) {
            ss << " (SKIPPED - below min " << min_size_absolute << " after rounding)";
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
        
        // Place bid order (buy limit order) only if size is above minimum after rounding
        std::string bid_order_id;
        if (quote_bid_after_rounding) {
            bid_order_id = generate_order_id() + "_BID";
            // Prices/sizes are already rounded, so send_order will pass them through
            // (MiniOMS will validate but not re-round since they're already aligned)
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
            std::stringstream bid_ss;
            bid_ss << "Skipping bid order - size (" << bid_size 
                   << ") below minimum (" << min_size_absolute << ") after rounding";
            get_logger().debug(bid_ss.str());
        }
        
        // Place ask order (sell limit order) only if size is above minimum after rounding
        std::string ask_order_id;
        if (quote_ask_after_rounding) {
            ask_order_id = generate_order_id() + "_ASK";
            // Prices/sizes are already rounded, so send_order will pass them through
            // (MiniOMS will validate but not re-round since they're already aligned)
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
            std::stringstream ask_ss;
            ask_ss << "Skipping ask order - size (" << ask_size 
                   << ") below minimum (" << min_size_absolute << ") after rounding";
            get_logger().debug(ask_ss.str());
        }
        
        // Track active order IDs and prices for cancellation on next update
        {
            std::lock_guard<std::mutex> lock(quote_update_mutex_);
            // Only update prices for sides that were actually quoted (use rounded prices)
            if (quote_bid_after_rounding) {
                last_quote_bid_price_ = bid_price;
            }
            if (quote_ask_after_rounding) {
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
    
    // Use CeFi-only inventory for risk management
    CeFiInventory cefi = calculate_cefi_inventory();
    double cefi_token1_tokens = 0.0;
    if (cefi.token1 != 0.0 && spot_price > 0.0) {
        auto& symbol_registry = ExchangeSymbolRegistry::get_instance();
        cefi_token1_tokens = symbol_registry.contracts_to_token_qty(
            exchange_, symbol_, cefi.token1, spot_price);
    }
    
    // Calculate total position value (CeFi only)
    double total_value = cefi.token0 + cefi_token1_tokens * spot_price;
    
    // Check if inventory exceeds limits
    double inventory_ratio_0 = cefi.token0 / total_value;
    double inventory_ratio_1 = (cefi_token1_tokens * spot_price) / total_value;
    
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

// CeFi-only inventory calculation (for GLFT model)
MarketMakingStrategy::CeFiInventory MarketMakingStrategy::calculate_cefi_inventory() const {
    CeFiInventory cefi;
    
    // Get CeFi positions from exchange via Mini PMS (queried through StrategyContainer)
    auto position_info = get_position(exchange_, symbol_);
    if (position_info.has_value()) {
        // token1 = perpetual position quantity in CONTRACTS
        cefi.token1 = position_info->qty;  // Already in contracts
    } else {
        // Fallback: use cached inventory delta if position query fails
        cefi.token1 = current_inventory_delta_.load();
    }
    
    // Get account balances for collateral (token0, e.g., USDC)
    auto balance_info = get_account_balance(exchange_, "USDT");  // Assuming USDT/USDC as collateral
    if (balance_info.has_value()) {
        cefi.token0 = balance_info->available + balance_info->locked;
    } else {
        // Try alternative collateral symbols
        balance_info = get_account_balance(exchange_, "USDC");
        if (balance_info.has_value()) {
            cefi.token0 = balance_info->available + balance_info->locked;
        } else {
            cefi.token0 = 0.0;
        }
    }
    
    return cefi;
}

// Combined inventory calculation
MarketMakingStrategy::CombinedInventory MarketMakingStrategy::calculate_combined_inventory(double spot_price) const {
    CombinedInventory inventory;
    
    // Get CeFi positions from exchange via Mini PMS (queried through StrategyContainer)
    // Positions come from exchange API/WebSocket, not from wallet directly
    // Flow: Exchange API -> Position Server -> Mini PMS -> Strategy (via get_position())
    
    // Query positions for the current symbol from the exchange
    // For perpetual futures: position quantity is in CONTRACTS (from exchange API)
    auto position_info = get_position(exchange_, symbol_);
    if (position_info.has_value()) {
        // token1_cefi = perpetual position quantity in CONTRACTS
        inventory.token1_cefi = position_info->qty;  // Already in contracts
    } else {
        // Fallback: use cached inventory delta if position query fails
        // Note: current_inventory_delta_ may be in tokens or contracts depending on source
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
    // Note: DeFi positions stored in defi_positions_ are in CONTRACTS
    // So we can directly add them to CeFi contracts
    {
        std::lock_guard<std::mutex> lock(defi_positions_mutex_);
        for (const auto& [pool_address, position] : defi_positions_) {
            inventory.token0_defi += position.token0_amount;
            inventory.token1_defi += position.token1_amount;  // Already in contracts
        }
    }
    
    // Calculate combined totals (both CeFi and DeFi are in contracts now)
    inventory.token0_total = inventory.token0_cefi + inventory.token0_defi;
    inventory.token1_total = inventory.token1_cefi + inventory.token1_defi;  // Net position in contracts
    
    return inventory;
}

double MarketMakingStrategy::calculate_micro_price(const proto::OrderBookSnapshot& orderbook, int num_levels) const {
    // Micro price = weighted mid price from top N levels
    // weighted_bid = sum(bid_price_i * bid_qty_i) / sum(bid_qty_i)
    // weighted_ask = sum(ask_price_i * ask_qty_i) / sum(ask_qty_i)
    // micro_price = (weighted_bid + weighted_ask) / 2
    
    if (orderbook.bids_size() == 0 || orderbook.asks_size() == 0) {
        return 0.0;
    }
    
    int bid_levels = std::min(num_levels, orderbook.bids_size());
    int ask_levels = std::min(num_levels, orderbook.asks_size());
    
    if (bid_levels == 0 || ask_levels == 0) {
        return 0.0;
    }
    
    // Calculate weighted bid price
    double weighted_bid_price = 0.0;
    double total_bid_qty = 0.0;
    for (int i = 0; i < bid_levels; ++i) {
        const auto& level = orderbook.bids(i);
        double price = level.price();
        double qty = level.qty();
        if (price > 0.0 && qty > 0.0) {
            weighted_bid_price += price * qty;
            total_bid_qty += qty;
        }
    }
    
    if (total_bid_qty <= 0.0) {
        return 0.0;
    }
    weighted_bid_price /= total_bid_qty;
    
    // Calculate weighted ask price
    double weighted_ask_price = 0.0;
    double total_ask_qty = 0.0;
    for (int i = 0; i < ask_levels; ++i) {
        const auto& level = orderbook.asks(i);
        double price = level.price();
        double qty = level.qty();
        if (price > 0.0 && qty > 0.0) {
            weighted_ask_price += price * qty;
            total_ask_qty += qty;
        }
    }
    
    if (total_ask_qty <= 0.0) {
        return 0.0;
    }
    weighted_ask_price /= total_ask_qty;
    
    // Micro price is the average of weighted bid and weighted ask
    double micro_price = (weighted_bid_price + weighted_ask_price) / 2.0;
    
    return micro_price;
}

double MarketMakingStrategy::get_micro_price_skew() const {
    // Returns normalized skew: (micro_price - mid_price) / mid_price
    // Positive = buying pressure (micro_price > mid_price)
    // Negative = selling pressure (micro_price < mid_price)
    
    std::lock_guard<std::mutex> lock(orderbook_mutex_);
    
    if (!orderbook_cached_ || best_bid_ <= 0.0 || best_ask_ <= 0.0) {
        return 0.0;  // No skew if no orderbook data
    }
    
    double mid_price = (best_bid_ + best_ask_) / 2.0;
    if (mid_price <= 0.0) {
        return 0.0;
    }
    
    double micro_price = calculate_micro_price(cached_orderbook_, 5);
    if (micro_price <= 0.0) {
        return 0.0;
    }
    
    // Calculate normalized skew
    double skew = (micro_price - mid_price) / mid_price;
    
    return skew;
}

double MarketMakingStrategy::get_orderbook_imbalance() const {
    // Returns orderbook imbalance: (bid_qty - ask_qty) / (bid_qty + ask_qty)
    // Range: [-1, 1]
    // Positive = more bid liquidity (selling pressure)
    // Negative = more ask liquidity (buying pressure)
    
    std::lock_guard<std::mutex> lock(orderbook_mutex_);
    
    if (!orderbook_cached_) {
        return 0.0;
    }
    
    // Sum quantities from top 5 levels
    double total_bid_qty = 0.0;
    double total_ask_qty = 0.0;
    
    int bid_levels = std::min(5, cached_orderbook_.bids_size());
    int ask_levels = std::min(5, cached_orderbook_.asks_size());
    
    for (int i = 0; i < bid_levels; ++i) {
        total_bid_qty += cached_orderbook_.bids(i).qty();
    }
    
    for (int i = 0; i < ask_levels; ++i) {
        total_ask_qty += cached_orderbook_.asks(i).qty();
    }
    
    if (total_bid_qty + total_ask_qty <= 0.0) {
        return 0.0;
    }
    
    // Calculate imbalance: (bid_qty - ask_qty) / (bid_qty + ask_qty)
    double imbalance = (total_bid_qty - total_ask_qty) / (total_bid_qty + total_ask_qty);
    
    return imbalance;
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
        
        // Store best bid/ask for quote validation and cache orderbook for micro price calculation
        {
            std::lock_guard<std::mutex> lock(orderbook_mutex_);
            best_bid_ = best_bid;
            best_ask_ = best_ask;
            
            // Cache orderbook for micro price calculation (top 5 levels)
            cached_orderbook_ = orderbook;
            orderbook_cached_ = true;
        }
    }
}

std::string MarketMakingStrategy::generate_order_id() const {
    // Use atomic counter to ensure uniqueness across all instances
    static std::atomic<uint64_t> order_id_counter_{0};
    
    uint64_t counter = order_id_counter_.fetch_add(1, std::memory_order_relaxed);
    
    std::ostringstream oss;
    oss << "MM_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
        << "_" << counter;
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