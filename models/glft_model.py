"""
Gueant-Lehalle-Fernandez-Tapia (GLFT) model implementation.
This model extends Avellaneda-Stoikov by considering finite inventory constraints
and execution costs, making it more realistic for practical market making.
"""
import math
import logging
import statistics
from typing import Dict, Tuple, Optional, List, Any
from decimal import Decimal, getcontext
from config import Config
from .base_model import BaseInventoryModel

# Set high precision for calculations
getcontext().prec = 50

logger = logging.getLogger(__name__)


class GLFTModel(BaseInventoryModel):
    """
    Gueant-Lehalle-Fernandez-Tapia model for market making with inventory constraints.
    
    This model extends the Avellaneda-Stoikov framework by:
    1. Considering finite inventory constraints
    2. Including execution costs
    3. More realistic risk management
    4. Terminal inventory penalties
    
    Key improvements over AS:
    - Finite inventory limits (more realistic)
    - Execution cost consideration
    - Better risk management
    - Terminal inventory optimization
    """
    
    def __init__(self, config: Config):
        """
        Initialize the GLFT model.
        
        Args:
            config: Configuration object with model parameters
        """
        super().__init__(config)
        
        # Core GLFT parameters
        self.risk_aversion = float(self.config.INVENTORY_RISK_AVERSION) if hasattr(self.config, 'INVENTORY_RISK_AVERSION') else 0.1
        self.target_inventory_ratio = float(self.config.TARGET_INVENTORY_RATIO) if hasattr(self.config, 'TARGET_INVENTORY_RATIO') else 0.5
        self.max_inventory_deviation = float(self.config.MAX_INVENTORY_DEVIATION) if hasattr(self.config, 'MAX_INVENTORY_DEVIATION') else 0.3
        self.base_spread = float(self.config.BASE_SPREAD) if hasattr(self.config, 'BASE_SPREAD') else 0.0001
        
        # GLFT-specific parameters
        self.execution_cost = float(getattr(self.config, 'EXECUTION_COST', 0.001))  # 0.1% execution cost
        self.inventory_penalty = float(getattr(self.config, 'INVENTORY_PENALTY', 0.05))  # Inventory holding penalty
        self.max_position_size = float(getattr(self.config, 'MAX_POSITION_SIZE', 0.1))  # 10% max position
        self.terminal_inventory_penalty = float(getattr(self.config, 'TERMINAL_INVENTORY_PENALTY', 0.2))  # Terminal penalty
        self.inventory_constraint_active = bool(getattr(self.config, 'INVENTORY_CONSTRAINT_ACTIVE', True))
        
        # Volatility calculation parameters
        self.volatility_window_size = int(self.config.VOLATILITY_WINDOW_SIZE) if hasattr(self.config, 'VOLATILITY_WINDOW_SIZE') else 20
        self.default_volatility = float(self.config.DEFAULT_VOLATILITY) if hasattr(self.config, 'DEFAULT_VOLATILITY') else 0.02
        self.max_volatility = float(self.config.MAX_VOLATILITY) if hasattr(self.config, 'MAX_VOLATILITY') else 2.0
        self.min_volatility = float(self.config.MIN_VOLATILITY) if hasattr(self.config, 'MIN_VOLATILITY') else 0.01
        
        logger.info(f"GLFT model initialized with risk aversion: {self.risk_aversion}, execution cost: {self.execution_cost}")
    
    def calculate_lp_ranges(
        self,
        token0_balance: int,
        token1_balance: int,
        spot_price: float,
        price_history: List[Dict[str, Any]],
        token_a_address: str,
        token_b_address: str,
        client: Any
    ) -> Dict[str, Any]:
        """
        Calculate optimal LP ranges using GLFT model.
        
        Args:
            token0_balance: Token0 balance in wei
            token1_balance: Token1 balance in wei
            spot_price: Current spot price (token1 per token0)
            price_history: Historical price data
            token_a_address: Token A address (our configured token A)
            token_b_address: Token B address (our configured token B)
            client: Uniswap client for additional data if needed
            
        Returns:
            Dictionary containing range calculations and metadata
        """
        try:
            # Validate inputs
            if not self.validate_inputs(token0_balance, token1_balance, spot_price, price_history):
                raise ValueError("Invalid input parameters")
            
            # Get token decimals dynamically
            token0_decimals = client.get_token_decimals(token_a_address)
            token1_decimals = client.get_token_decimals(token_b_address)
            
            # Calculate current inventory ratio
            current_ratio = self.calculate_inventory_ratio(
                token0_balance, token1_balance, spot_price, token0_decimals, token1_decimals
            )
            
            # Calculate inventory deviation from target
            inventory_deviation = abs(current_ratio - self.target_inventory_ratio)
            
            # Calculate volatility
            volatility = self.calculate_volatility(price_history, self.volatility_window_size)
            
            # Calculate current inventory levels (normalized)
            token0_amount = token0_balance / (10 ** token0_decimals)
            token1_amount = token1_balance / (10 ** token1_decimals)
            total_value = token0_amount * spot_price + token1_amount
            
            # Normalize inventory to portfolio value
            normalized_inventory_0 = (token0_amount * spot_price) / total_value if total_value > 0 else 0
            normalized_inventory_1 = token1_amount / total_value if total_value > 0 else 0
            
            # Calculate GLFT optimal ranges
            range_a, range_b = self._calculate_glft_ranges(
                normalized_inventory_0, normalized_inventory_1, volatility, spot_price
            )
            
            # Apply inventory constraints if enabled
            if self.inventory_constraint_active:
                range_a, range_b = self._apply_inventory_constraints(
                    range_a, range_b, normalized_inventory_0, normalized_inventory_1
                )
            
            # Apply min/max constraints
            range_a = max(range_a, self.config.MIN_RANGE_PERCENTAGE / 100.0)
            range_a = min(range_a, self.config.MAX_RANGE_PERCENTAGE / 100.0)
            range_b = max(range_b, self.config.MIN_RANGE_PERCENTAGE / 100.0)
            range_b = min(range_b, self.config.MAX_RANGE_PERCENTAGE / 100.0)
            
            # Log when constraints are applied
            if range_a == self.config.MIN_RANGE_PERCENTAGE / 100.0 or range_a == self.config.MAX_RANGE_PERCENTAGE / 100.0:
                logger.info(f"GLFT Range A constrained to {range_a:.3f} by min/max limits")
            if range_b == self.config.MIN_RANGE_PERCENTAGE / 100.0 or range_b == self.config.MAX_RANGE_PERCENTAGE / 100.0:
                logger.info(f"GLFT Range B constrained to {range_b:.3f} by min/max limits")
            
            return {
                'range_a_percentage': range_a * 100.0,  # Convert to percentage
                'range_b_percentage': range_b * 100.0,  # Convert to percentage
                'inventory_ratio': current_ratio,
                'target_ratio': self.target_inventory_ratio,
                'deviation': inventory_deviation,
                'volatility': volatility,
                'model_metadata': {
                    'model_name': self.model_name,
                    'risk_aversion': self.risk_aversion,
                    'execution_cost': self.execution_cost,
                    'inventory_penalty': self.inventory_penalty,
                    'max_position_size': self.max_position_size,
                    'inventory_constraint_active': self.inventory_constraint_active,
                    'normalized_inventory_0': normalized_inventory_0,
                    'normalized_inventory_1': normalized_inventory_1,
                    'glft_formula_used': True
                }
            }
            
        except Exception as e:
            logger.error(f"Error in GLFT range calculation: {e}")
            # Return default ranges on error
            return {
                'range_a_percentage': self.config.MIN_RANGE_PERCENTAGE,
                'range_b_percentage': self.config.MIN_RANGE_PERCENTAGE,
                'inventory_ratio': 0.5,
                'target_ratio': self.target_inventory_ratio,
                'deviation': 0.0,
                'volatility': self.default_volatility,
                'model_metadata': {
                    'model_name': self.model_name,
                    'error': str(e),
                    'fallback_used': True
                }
            }
    
    def _calculate_glft_ranges(
        self,
        normalized_inventory_0: float,
        normalized_inventory_1: float,
        volatility: float,
        spot_price: float
    ) -> Tuple[float, float]:
        """
        Calculate ranges using GLFT model formula.
        
        The GLFT model extends AS by considering:
        1. Finite inventory constraints
        2. Execution costs
        3. Inventory holding penalties
        4. Terminal inventory optimization
        
        Args:
            normalized_inventory_0: Normalized token0 inventory (0-1)
            normalized_inventory_1: Normalized token1 inventory (0-1)
            volatility: Market volatility
            spot_price: Current spot price
            
        Returns:
            Tuple of (range_a, range_b) as decimals
        """
        try:
            # Calculate inventory skew
            inventory_skew = normalized_inventory_0 - self.target_inventory_ratio
            
            # GLFT base spread calculation
            # Includes execution cost and inventory penalty
            base_spread_component = self.base_spread + self.execution_cost
            
            # Inventory risk component (similar to AS but with constraints)
            inventory_risk_component = (
                self.risk_aversion * (volatility ** 2) * abs(inventory_skew) +
                self.inventory_penalty * abs(inventory_skew)
            )
            
            # Terminal inventory penalty (encourages rebalancing)
            terminal_penalty_component = (
                self.terminal_inventory_penalty * (inventory_skew ** 2)
            )
            
            # Total spread
            total_spread = base_spread_component + inventory_risk_component + terminal_penalty_component
            
            # Calculate ranges based on inventory position and constraints
            if inventory_skew > 0:
                # We have too much token0, make token0 range narrow (encourage selling)
                # But consider execution costs
                range_a = total_spread * 0.5 * (1 + self.execution_cost)
                range_b = total_spread * 2.0 * (1 + self.execution_cost)
            else:
                # We have too much token1, make token1 range narrow (encourage selling)
                range_a = total_spread * 2.0 * (1 + self.execution_cost)
                range_b = total_spread * 0.5 * (1 + self.execution_cost)
            
            # Apply finite inventory constraints
            range_a = self._apply_finite_inventory_constraint(range_a, normalized_inventory_0)
            range_b = self._apply_finite_inventory_constraint(range_b, normalized_inventory_1)
            
            logger.debug(f"GLFT ranges calculated: A={range_a:.4f}, B={range_b:.4f}, "
                        f"skew={inventory_skew:.4f}, volatility={volatility:.4f}, "
                        f"execution_cost={self.execution_cost:.4f}")
            
            return range_a, range_b
            
        except Exception as e:
            logger.error(f"Error in GLFT range calculation: {e}")
            # Return default ranges
            return self.base_spread, self.base_spread
    
    def _apply_finite_inventory_constraint(self, range_size: float, inventory_level: float) -> float:
        """
        Apply finite inventory constraint to range size.
        
        When inventory is close to limits, adjust ranges to prevent
        exceeding maximum position sizes.
        
        Args:
            range_size: Original range size
            inventory_level: Current inventory level (0-1)
            
        Returns:
            Constraint-adjusted range size
        """
        try:
            # Calculate distance from inventory limits
            distance_from_limit = min(inventory_level, 1 - inventory_level)
            
            # If we're close to inventory limits, reduce range size
            if distance_from_limit < self.max_position_size:
                # Reduce range size to prevent exceeding limits
                constraint_factor = distance_from_limit / self.max_position_size
                adjusted_range = range_size * constraint_factor
                
                logger.debug(f"Applied inventory constraint: {range_size:.4f} -> {adjusted_range:.4f}, "
                           f"inventory_level={inventory_level:.4f}, distance_from_limit={distance_from_limit:.4f}")
                
                return adjusted_range
            
            return range_size
            
        except Exception as e:
            logger.error(f"Error applying inventory constraint: {e}")
            return range_size
    
    def _apply_inventory_constraints(
        self,
        range_a: float,
        range_b: float,
        normalized_inventory_0: float,
        normalized_inventory_1: float
    ) -> Tuple[float, float]:
        """
        Apply additional inventory constraints to ranges.
        
        Args:
            range_a: Range A size
            range_b: Range B size
            normalized_inventory_0: Normalized token0 inventory
            normalized_inventory_1: Normalized token1 inventory
            
        Returns:
            Tuple of constraint-adjusted ranges
        """
        try:
            # Check if we're at inventory limits
            at_token0_limit = normalized_inventory_0 >= (1 - self.max_position_size)
            at_token1_limit = normalized_inventory_1 >= (1 - self.max_position_size)
            
            # Adjust ranges based on inventory limits
            if at_token0_limit:
                # At token0 limit, reduce range A (selling token0)
                range_a *= 0.5
                logger.info(f"At token0 inventory limit, reducing range A to {range_a:.4f}")
            
            if at_token1_limit:
                # At token1 limit, reduce range B (selling token1)
                range_b *= 0.5
                logger.info(f"At token1 inventory limit, reducing range B to {range_b:.4f}")
            
            return range_a, range_b
            
        except Exception as e:
            logger.error(f"Error applying inventory constraints: {e}")
            return range_a, range_b
    
    def get_model_info(self) -> Dict[str, Any]:
        """
        Get information about the GLFT model.
        
        Returns:
            Dictionary with model information
        """
        return {
            'name': 'Gueant-Lehalle-Fernandez-Tapia Model',
            'description': 'Advanced market making model with inventory constraints and execution costs',
            'version': '1.0.0',
            'parameters': {
                'risk_aversion': self.risk_aversion,
                'target_inventory_ratio': self.target_inventory_ratio,
                'max_inventory_deviation': self.max_inventory_deviation,
                'base_spread': self.base_spread,
                'execution_cost': self.execution_cost,
                'inventory_penalty': self.inventory_penalty,
                'max_position_size': self.max_position_size,
                'terminal_inventory_penalty': self.terminal_inventory_penalty,
                'inventory_constraint_active': self.inventory_constraint_active,
                'volatility_window_size': self.volatility_window_size,
                'default_volatility': self.default_volatility,
                'max_volatility': self.max_volatility,
                'min_volatility': self.min_volatility
            },
            'features': [
                'Finite inventory constraints',
                'Execution cost consideration',
                'Inventory holding penalties',
                'Terminal inventory optimization',
                'Dynamic range adjustment based on inventory levels',
                'Risk management with position size limits',
                'Volatility-adjusted spreads',
                'More realistic than basic AS model'
            ],
            'improvements_over_as': [
                'Considers finite inventory (AS assumes infinite)',
                'Includes execution costs (AS ignores them)',
                'Better risk management with position limits',
                'Terminal inventory penalties encourage rebalancing',
                'More realistic for practical market making'
            ]
        }
