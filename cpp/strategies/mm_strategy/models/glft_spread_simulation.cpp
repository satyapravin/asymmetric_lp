#include "glft_target.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

/**
 * Simulation to show how GLFT spreads change with inventory
 */
void simulate_spread_changes() {
    std::cout << "=== GLFT Spread Simulation: Inventory Impact ===\n\n";
    
    // Setup GLFT model with default config
    GlftTarget::Config config;
    // Calibrated for ~100 bps at 10% inventory
    config.risk_aversion = 0.1;  // Calibrated for target spread
    config.target_inventory_ratio = 0.0;  // Delta-neutral
    config.base_spread = 0.0002;          // 0.02% = 2 bps
    config.execution_cost = 0.0;
    config.inventory_penalty = 0.024;  // Calibrated for target spread
    config.terminal_inventory_penalty = 0.1;
    config.max_position_size = 0.1;       // 10% max
    
    GlftTarget glft(config);
    
    // Simulation parameters (production-like values)
    double spot_price = 100000.0;  // BTCUSDT at $100K
    double balance_btc = 1.0;      // 1 BTC balance (collateral)
    double collateral_usd = balance_btc * spot_price;  // $100K collateral (token0)
    double volatility = 0.8;      // 80% annualized volatility (typical for crypto)
    
    std::cout << "Parameters:\n";
    std::cout << "  Balance: " << std::fixed << std::setprecision(4) << balance_btc << " BTC\n";
    std::cout << "  Collateral (token0): $" << std::setprecision(2) << collateral_usd << "\n";
    std::cout << "  Spot Price: $" << spot_price << "\n";
    std::cout << "  Volatility: " << std::setprecision(1) << (volatility * 100) << "% annualized\n";
    std::cout << "  Base Spread: " << std::setprecision(2) << (config.base_spread * 100) << "% (" 
              << (config.base_spread * 10000) << " bps)\n\n";
    
    std::cout << "Inventory (BTC) | Position Value | Target Offset | Effective Spread | Spread Change\n";
    std::cout << "-----------------|----------------|---------------|------------------|---------------\n";
    
    // Simulate different inventory levels (in BTC)
    double base_spread_bps = config.base_spread * 10000; // Convert to basis points
    
    for (double position_btc = -0.2; position_btc <= 0.2; position_btc += 0.05) {
        // Calculate combined inventory (as GLFT expects)
        double inventory_0 = collateral_usd;  // Collateral in USD
        double inventory_1 = position_btc;     // Position in BTC
        
        // Get target offset from GLFT (actual implementation)
        double target_offset = glft.compute_target(inventory_0, inventory_1, spot_price, volatility);
        
        // Calculate spread using actual GLFT normalization logic
        double reference_value = std::max(inventory_0, 1.0);
        double inventory_skew = inventory_1;  // Position in BTC
        double normalized_skew = inventory_skew / reference_value;  // As GLFT does it
        
        // Calculate components (matching GLFT logic exactly)
        double base_spread_component = config.base_spread + config.execution_cost;
        double inventory_risk_component = (
            config.risk_aversion * (volatility * volatility) * std::abs(normalized_skew) +
            config.inventory_penalty * std::abs(normalized_skew)
        );
        double terminal_penalty_component = (
            config.terminal_inventory_penalty * (normalized_skew * normalized_skew)
        );
        
        // Total spread adjustment = base + risk components
        double spread_adjustment = inventory_risk_component + terminal_penalty_component;
        double effective_spread = base_spread_component + spread_adjustment;
        double effective_spread_bps = effective_spread * 10000;
        double spread_change_bps = effective_spread_bps - base_spread_bps;
        
        double position_value = position_btc * spot_price;
        
        std::cout << std::setw(17) << std::right << std::fixed << std::setprecision(4) 
                  << position_btc << " | $"
                  << std::setw(14) << std::right << std::setprecision(2) << position_value << " | "
                  << std::setw(13) << std::right << std::setprecision(6) << target_offset << " | "
                  << std::setw(16) << std::right << std::setprecision(2) << effective_spread_bps << " bps | "
                  << std::setw(13) << std::right << std::setprecision(2) << spread_change_bps << " bps\n";
    }
    
    std::cout << "\n=== Key Insight: 0.1 BTC Inventory (10% of 1 BTC balance) ===\n\n";
    
    // Show specific case: 0 BTC vs 0.1 BTC inventory
    double position_0 = 0.0;
    double position_01 = 0.1;  // 0.1 BTC position
    
    double offset_0 = glft.compute_target(collateral_usd, position_0, spot_price, volatility);
    double offset_01 = glft.compute_target(collateral_usd, position_01, spot_price, volatility);
    
    // Calculate spreads using FIXED GLFT normalization (position value % of collateral)
    double ref_val = collateral_usd;
    double position_value_0 = position_0 * spot_price;
    double position_value_01 = position_01 * spot_price;
    double skew_0 = position_value_0 / ref_val;  // 0%
    double skew_01 = position_value_01 / ref_val;  // 10%
    
    double base = config.base_spread;
    double risk_0 = config.risk_aversion * volatility * volatility * std::abs(skew_0) + 
                    config.inventory_penalty * std::abs(skew_0);
    double risk_01 = config.risk_aversion * volatility * volatility * std::abs(skew_01) + 
                     config.inventory_penalty * std::abs(skew_01);
    double term_0 = config.terminal_inventory_penalty * (skew_0 * skew_0);
    double term_01 = config.terminal_inventory_penalty * (skew_01 * skew_01);
    
    double spread_0 = (base + risk_0 + term_0) * 10000;  // in bps
    double spread_01 = (base + risk_01 + term_01) * 10000;  // in bps
    
    std::cout << "FIXED IMPLEMENTATION (normalizes by position value % of collateral):\n\n";
    std::cout << "Delta-Neutral (0 BTC inventory):\n";
    std::cout << "  Position: 0 BTC\n";
    std::cout << "  Normalized Skew: " << std::setprecision(4) << skew_0 << " (0% of collateral)\n";
    std::cout << "  Target Offset: " << std::setprecision(4) << offset_0 << " BTC\n";
    std::cout << "  Effective Spread: " << std::setprecision(2) << spread_0 << " bps\n";
    std::cout << "  Spread in $: $" << (spread_0 / 10000.0 * spot_price) << "\n\n";
    
    std::cout << "With 0.1 BTC Long Position:\n";
    std::cout << "  Position: " << std::setprecision(4) << position_01 << " BTC\n";
    std::cout << "  Position Value: $" << std::setprecision(2) << position_value_01 << "\n";
    std::cout << "  Normalized Skew: " << std::setprecision(4) << skew_01 << " (10% of collateral)\n";
    std::cout << "  Target Offset: " << std::setprecision(4) << offset_01 << " BTC (negative = reduce position)\n";
    std::cout << "  Inventory Risk Component: " << std::setprecision(6) << risk_01 << "\n";
    std::cout << "  Terminal Penalty Component: " << std::setprecision(6) << term_01 << "\n";
    std::cout << "  Effective Spread: " << std::setprecision(2) << spread_01 << " bps\n";
    std::cout << "  Spread in $: $" << (spread_01 / 10000.0 * spot_price) << "\n\n";
    
    std::cout << "Change:\n";
    std::cout << "  Spread Increase: " << std::setprecision(2) << (spread_01 - spread_0) << " bps\n";
    std::cout << "  Spread Increase: $" << std::setprecision(2) << ((spread_01 - spread_0) / 10000.0 * spot_price) << "\n";
    if (spread_0 > 0) {
        std::cout << "  Percentage Increase: " << std::setprecision(1) 
                  << (((spread_01 - spread_0) / spread_0) * 100) << "%\n";
    }
    
    std::cout << "\n=== Interpretation ===\n";
    std::cout << "When inventory increases by 0.1 BTC (10% of $100K collateral):\n";
    std::cout << "- Base spread: " << std::setprecision(0) << spread_0 << " bps ($" << std::setprecision(2) << (spread_0 / 10000.0 * spot_price) << ")\n";
    std::cout << "- With 10% inventory: " << std::setprecision(0) << spread_01 << " bps ($" << std::setprecision(2) << (spread_01 / 10000.0 * spot_price) << ")\n";
    std::cout << "- Spread widens by " << std::setprecision(0) << (spread_01 - spread_0) << " bps ($" << std::setprecision(2) << ((spread_01 - spread_0) / 10000.0 * spot_price) << ") to encourage rebalancing\n";
    std::cout << "- This makes it more expensive to buy (wider ask) and encourages selling\n";
    std::cout << "\nParameters used:\n";
    std::cout << "  Base Spread: " << std::setprecision(2) << (config.base_spread * 10000) << " bps\n";
    std::cout << "  Risk Aversion: " << config.risk_aversion << "\n";
    std::cout << "  Inventory Penalty: " << config.inventory_penalty << "\n";
    std::cout << "  Terminal Penalty: " << config.terminal_inventory_penalty << "\n";
}

int main() {
    simulate_spread_changes();
    return 0;
}

