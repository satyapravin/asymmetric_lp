#include "market_making_strategy_config.hpp"
#include "../../utils/logging/logger.hpp"
#include <iomanip>

void MarketMakingStrategyConfig::load_from_config(
    const config::ProcessConfigManager& config_manager,
    const std::string& section) {
    
    // GLFT Model Parameters
    glft.risk_aversion = config_manager.get_double(section, "glft_risk_aversion", glft.risk_aversion);
    glft.target_inventory_ratio = config_manager.get_double(section, "glft_target_inventory_ratio", glft.target_inventory_ratio);
    glft.base_spread = config_manager.get_double(section, "glft_base_spread", glft.base_spread);
    glft.execution_cost = config_manager.get_double(section, "glft_execution_cost", glft.execution_cost);
    glft.inventory_penalty = config_manager.get_double(section, "glft_inventory_penalty", glft.inventory_penalty);
    glft.terminal_inventory_penalty = config_manager.get_double(section, "glft_terminal_inventory_penalty", glft.terminal_inventory_penalty);
    glft.max_position_size = config_manager.get_double(section, "glft_max_position_size", glft.max_position_size);
    glft.inventory_constraint_active = config_manager.get_bool(section, "glft_inventory_constraint_active", glft.inventory_constraint_active);
    
    // Quote Sizing Parameters
    leverage = config_manager.get_double(section, "leverage", leverage);
    base_quote_size_pct = config_manager.get_double(section, "base_quote_size_pct", base_quote_size_pct);
    min_quote_size_pct = config_manager.get_double(section, "min_quote_size_pct", min_quote_size_pct);
    max_quote_size_pct = config_manager.get_double(section, "max_quote_size_pct", max_quote_size_pct);
    
    // Quote Update Throttling
    min_price_change_bps = config_manager.get_double(section, "min_price_change_bps", min_price_change_bps);
    min_inventory_change_pct = config_manager.get_double(section, "min_inventory_change_pct", min_inventory_change_pct);
    quote_update_interval_ms = config_manager.get_int(section, "quote_update_interval_ms", quote_update_interval_ms);
    min_quote_price_change_bps = config_manager.get_double(section, "min_quote_price_change_bps", min_quote_price_change_bps);
    
    // Risk Management
    min_spread_bps = config_manager.get_double(section, "min_spread_bps", min_spread_bps);
    max_position_size = config_manager.get_double(section, "max_position_size", max_position_size);
    
    // Volatility Calculation
    ewma_decay_factor = config_manager.get_double(section, "ewma_decay_factor", ewma_decay_factor);
    
    // Micro Price Skew
    micro_price_skew_alpha = config_manager.get_double(section, "micro_price_skew_alpha", micro_price_skew_alpha);
    
    // Net Inventory Skew (CeFi + DeFi combined)
    net_inventory_skew_gamma = config_manager.get_double(section, "net_inventory_skew_gamma", net_inventory_skew_gamma);
    
    // DeFi Inventory Flow Weights
    defi_flow_5s_weight = config_manager.get_double(section, "defi_flow_5s_weight", defi_flow_5s_weight);
    defi_flow_1m_weight = config_manager.get_double(section, "defi_flow_1m_weight", defi_flow_1m_weight);
    defi_flow_5m_weight = config_manager.get_double(section, "defi_flow_5m_weight", defi_flow_5m_weight);
}

bool MarketMakingStrategyConfig::load_from_file(
    const std::string& config_file,
    const std::string& section) {
    
    config::ProcessConfigManager config_manager;
    if (!config_manager.load_config(config_file)) {
        logging::Logger logger("STRATEGY_CONFIG");
        logger.error("Failed to load config file: " + config_file);
        return false;
    }
    
    load_from_config(config_manager, section);
    return true;
}

void MarketMakingStrategyConfig::print() const {
    logging::Logger logger("STRATEGY_CONFIG");
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "=== Market Making Strategy Configuration ===" << std::endl
       << "\n[GLFT Model]" << std::endl
       << "  Risk Aversion: " << glft.risk_aversion << std::endl
       << "  Target Inventory Ratio: " << glft.target_inventory_ratio << std::endl
       << "  Base Spread: " << glft.base_spread << " (" << (glft.base_spread * 10000) << " bps)" << std::endl
       << "  Execution Cost: " << glft.execution_cost << std::endl
       << "  Inventory Penalty: " << glft.inventory_penalty << std::endl
       << "  Terminal Inventory Penalty: " << glft.terminal_inventory_penalty << std::endl
       << "  Max Position Size: " << glft.max_position_size << " (" << (glft.max_position_size * 100) << "%)" << std::endl
       << "  Inventory Constraint Active: " << (glft.inventory_constraint_active ? "true" : "false") << std::endl
       << "\n[Quote Sizing]" << std::endl
       << "  Leverage: " << leverage << "x" << std::endl
       << "  Base Quote Size: " << (base_quote_size_pct * 100) << "%" << std::endl
       << "  Min Quote Size: " << (min_quote_size_pct * 100) << "%" << std::endl
       << "  Max Quote Size: " << (max_quote_size_pct * 100) << "%" << std::endl
       << "\n[Quote Update Throttling]" << std::endl
       << "  Min Price Change: " << min_price_change_bps << " bps" << std::endl
       << "  Min Inventory Change: " << min_inventory_change_pct << "%" << std::endl
       << "  Update Interval: " << quote_update_interval_ms << " ms" << std::endl
       << "  Min Quote Price Change: " << min_quote_price_change_bps << " bps" << std::endl
       << "\n[Risk Management]" << std::endl
       << "  Min Spread: " << min_spread_bps << " bps" << std::endl
       << "  Max Position Size: " << max_position_size << std::endl
       << "\n[Volatility]" << std::endl
       << "  EWMA Decay Factor: " << ewma_decay_factor << std::endl
       << "\n[Micro Price Skew]" << std::endl
       << "  Alpha: " << micro_price_skew_alpha << std::endl
       << "\n[Net Inventory Skew]" << std::endl
       << "  Gamma: " << net_inventory_skew_gamma << std::endl
       << "  (Net inventory = CeFi + DeFi flow, both in contracts, normalized to % of collateral)" << std::endl
       << "\n[DeFi Inventory Flow Weights]" << std::endl
       << "  5s Weight: " << defi_flow_5s_weight << std::endl
       << "  1m Weight: " << defi_flow_1m_weight << " (primary)" << std::endl
       << "  5m Weight: " << defi_flow_5m_weight << std::endl
       << "===========================================";
    logger.info(ss.str());
}

