#include "glft_target.hpp"
#include <cmath>
#include <algorithm>

GlftTarget::GlftTarget() : config_() {}

GlftTarget::GlftTarget(const Config& config) : config_(config) {}

double GlftTarget::compute_target(
    double combined_inventory_0,
    double combined_inventory_1,
    double spot_price,
    double volatility
) const {
    if (spot_price <= 0.0 || volatility < 0.0) {
        return 0.0; // Invalid inputs
    }
    
    // For perpetual futures: inventory = active positions + DeFi delta
    // token0 = collateral (not part of inventory)
    // token1 = perpetual position (this is the inventory we track)
    // For delta-neutral: target position = 0, so inventory_skew = current_position - 0
    
    // Calculate inventory as position delta (token1)
    double current_position = combined_inventory_1; // Perpetual position quantity in BTC
    double target_position = 0.0; // Delta-neutral target
    
    // Calculate inventory skew (deviation from target)
    // For perps: inventory_skew = current_position - target_position
    double inventory_skew = current_position - target_position;
    
    // Normalize inventory skew for risk calculations
    // Convert position to USD value, then normalize by collateral
    // This gives position as percentage of collateral (0.0 to 1.0+)
    double reference_value = std::max(combined_inventory_0, 1.0); // Collateral in USD
    double position_value_usd = inventory_skew * spot_price; // Position value in USD
    double normalized_inventory_skew = position_value_usd / reference_value; // Position % of collateral
    
    // GLFT base spread component
    double base_spread_component = config_.base_spread + config_.execution_cost;
    
    // Inventory risk component (based on position deviation from target)
    double inventory_risk_component = (
        config_.risk_aversion * (volatility * volatility) * std::abs(normalized_inventory_skew) +
        config_.inventory_penalty * std::abs(normalized_inventory_skew)
    );
    
    // Terminal inventory penalty (encourages rebalancing to zero)
    double terminal_penalty_component = (
        config_.terminal_inventory_penalty * (normalized_inventory_skew * normalized_inventory_skew)
    );
    
    // Total spread/adjustment
    double total_adjustment = base_spread_component + inventory_risk_component + terminal_penalty_component;
    
    // Calculate target inventory offset
    // Negative offset means we want to reduce position (move toward zero)
    // Positive offset means we want to increase position (move away from zero)
    // For delta-neutral: we want to reduce any non-zero position
    double target_offset = -normalized_inventory_skew * total_adjustment;
    
    // Apply finite inventory constraints if enabled
    if (config_.inventory_constraint_active) {
        // For perps: normalized_inventory_0 represents collateral ratio, normalized_inventory_1 represents position ratio
        double normalized_inventory_0 = combined_inventory_0 / reference_value;
        double normalized_inventory_1 = std::abs(current_position) / reference_value;
        target_offset = apply_finite_inventory_constraint(target_offset, normalized_inventory_0, normalized_inventory_1);
    }
    
    // Clamp to max position size (as fraction of reference value)
    target_offset = std::clamp(target_offset, -config_.max_position_size, config_.max_position_size);
    
    // Convert normalized offset to absolute position offset
    // target_offset is normalized (e.g., -0.1 means reduce by 10% of collateral value)
    // Convert to BTC units: multiply by collateral value, then divide by spot price
    // This gives desired change in position quantity (token1/BTC units)
    // Negative = reduce position (move toward zero), Positive = increase position
    double position_offset_usd = target_offset * reference_value;  // USD value to adjust
    double position_offset = position_offset_usd / spot_price;  // Convert to BTC
    
    return position_offset;
}

double GlftTarget::compute_target(double desired_offset) const {
    // For backward compatibility, return the desired offset
    // In full implementation, this would use current inventory and market conditions
    return desired_offset;
}

double GlftTarget::apply_finite_inventory_constraint(
    double target_offset,
    double normalized_inventory_0,
    double normalized_inventory_1
) const {
    // If we're at max inventory in one direction, reduce target offset
    if (normalized_inventory_0 > (1.0 - config_.max_position_size)) {
        // Too much token0, reduce positive offset
        target_offset = std::min(target_offset, 0.0);
    }
    if (normalized_inventory_1 > (1.0 - config_.max_position_size)) {
        // Too much token1, reduce negative offset
        target_offset = std::max(target_offset, 0.0);
    }
    
    return target_offset;
}

