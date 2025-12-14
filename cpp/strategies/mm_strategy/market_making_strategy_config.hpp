#pragma once
#include <string>
#include "../../utils/config/process_config_manager.hpp"

/**
 * Market Making Strategy Configuration
 * 
 * Holds all configurable parameters for the MarketMakingStrategy.
 * Can be loaded from INI config files using ProcessConfigManager.
 */
struct MarketMakingStrategyConfig {
    // GLFT Model Parameters
    struct GLFTConfig {
        double risk_aversion{0.1};
        double target_inventory_ratio{0.0};
        double base_spread{0.0002};
        double execution_cost{0.0};
        double inventory_penalty{0.024};
        double terminal_inventory_penalty{0.05};
        double max_position_size{0.5};
        bool inventory_constraint_active{false};
    } glft;
    
    // Quote Sizing Parameters
    double leverage{1.0};
    double base_quote_size_pct{0.02};      // 2% of leveraged balance
    double min_quote_size_pct{0.005};      // 0.5% of leveraged balance
    double max_quote_size_pct{0.25};       // 25% of leveraged balance
    
    // Quote Update Throttling
    double min_price_change_bps{5.0};      // 5 basis points
    double min_inventory_change_pct{1.0};  // 1%
    int quote_update_interval_ms{5000};     // 5 seconds
    double min_quote_price_change_bps{2.0}; // 2 basis points
    
    // Risk Management
    double min_spread_bps{5.0};            // 5 basis points minimum spread
    double max_position_size{100.0};       // Maximum position size
    
    // Volatility Calculation
    double ewma_decay_factor{0.94};         // EWMA lambda parameter
    
    // Micro Price Skew
    double micro_price_skew_alpha{1.0};     // Alpha parameter to multiply micro price skew (default: 1.0 = full effect)
    
    // Net Inventory Skew (CeFi + DeFi combined)
    double net_inventory_skew_gamma{0.5};  // Gamma parameter to multiply net inventory skew (default: 0.5)
                                           // Net inventory = CeFi inventory + DeFi flow (both in contracts)
                                           // Normalized to % of collateral for skew calculation
    
    // DeFi Inventory Flow Weights
    double defi_flow_5s_weight{0.2};      // Weight for 5-second flow (default: 0.2)
    double defi_flow_1m_weight{0.6};      // Weight for 1-minute flow (default: 0.6, primary)
    double defi_flow_5m_weight{0.2};       // Weight for 5-minute flow (default: 0.2)
    
    /**
     * Load configuration from ProcessConfigManager
     * 
     * @param config_manager The config manager instance
     * @param section The config section name (default: "market_making_strategy")
     */
    void load_from_config(const config::ProcessConfigManager& config_manager,
                         const std::string& section = "market_making_strategy");
    
    /**
     * Load configuration from a config file
     * 
     * @param config_file Path to config file
     * @param section The config section name (default: "market_making_strategy")
     */
    bool load_from_file(const std::string& config_file,
                       const std::string& section = "market_making_strategy");
    
    /**
     * Print configuration to stdout (for debugging)
     */
    void print() const;
};

