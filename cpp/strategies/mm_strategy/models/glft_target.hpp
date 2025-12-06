#pragma once
#include <cmath>
#include <algorithm>

/**
 * GLFT (Guéant-Lehalle-Fernandez-Tapia) Target Inventory Model
 * 
 * Calculates optimal target inventory based on combined CeFi and DeFi inventory.
 * The GLFT model extends Avellaneda-Stoikov by considering:
 * - Finite inventory constraints
 * - Execution costs
 * - Inventory holding penalties
 * - Terminal inventory optimization
 */
class GlftTarget {
public:
    struct Config {
        double risk_aversion{0.1};           // Risk aversion parameter (calibrated for ~100 bps at 10% inventory)
        double target_inventory_ratio{0.0};  // Target inventory ratio: 0.0 = delta-neutral for perps (zero net position), 0.5 = balanced for spot. Note: inventory = active positions + DeFi delta
        double base_spread{0.0002};         // Base spread (0.02% = 2 basis points) - tighter for competitive market making
        double execution_cost{0.0};         // Execution cost (0.0% for maker orders in crypto)
        double inventory_penalty{0.024};    // Inventory holding penalty (calibrated for ~100 bps at 10% inventory)
        double terminal_inventory_penalty{0.05}; // Terminal inventory penalty (5%)
        double max_position_size{0.5};       // Max position size (50%)
        bool inventory_constraint_active{false}; // Enable finite inventory constraints
    };

    GlftTarget();
    explicit GlftTarget(const Config& config);
    
    /**
     * Compute target inventory using GLFT model
     * 
     * For perpetual futures: inventory = active positions + DeFi delta
     * - token0 = collateral (not part of inventory)
     * - token1 = perpetual position (this is the inventory we track)
     * - For delta-neutral: target position = 0
     * 
     * @param combined_inventory_0 Combined token0 (collateral) in base units
     * @param combined_inventory_1 Combined token1 (perpetual position) in base units
     * @param spot_price Current spot price (token1 per token0)
     * @param volatility Market volatility (annualized)
     * @return Target position offset in token1 units (desired change in position quantity)
     *         Negative = reduce position (move toward zero), Positive = increase position
     */
    double compute_target(
        double combined_inventory_0,
        double combined_inventory_1,
        double spot_price,
        double volatility
    ) const;
    
    /**
     * Compute target inventory using simplified interface (for backward compatibility)
     * 
     * @param desired_offset Desired inventory offset
     * @return Target inventory offset (same as input for now, but can be enhanced)
     */
    double compute_target(double desired_offset) const;
    
    // Configuration setters
    void set_risk_aversion(double risk_aversion) { config_.risk_aversion = risk_aversion; }
    void set_target_inventory_ratio(double ratio) { config_.target_inventory_ratio = ratio; }
    void set_base_spread(double spread) { config_.base_spread = spread; }
    void set_execution_cost(double cost) { config_.execution_cost = cost; }
    void set_inventory_penalty(double penalty) { config_.inventory_penalty = penalty; }
    void set_max_position_size(double max_size) { config_.max_position_size = max_size; }
    
    const Config& get_config() const { return config_; }

private:
    Config config_;
    
    /**
     * Apply finite inventory constraint to target offset
     * 
     * When inventory is close to limits, adjust target to prevent over-leveraging
     */
    double apply_finite_inventory_constraint(
        double target_offset,
        double normalized_inventory_0,
        double normalized_inventory_1
    ) const;
};
