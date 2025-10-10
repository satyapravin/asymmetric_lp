"""
Avellaneda-Stoikov inventory management model implementation.
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


class AvellanedaStoikovModel(BaseInventoryModel):
    """
    Avellaneda-Stoikov inventory management model for dynamic LP range calculation.
    
    This model implements the classic Avellaneda-Stoikov market making theory
    adapted for Uniswap V3 liquidity provision.
    """
    
    def __init__(self, config: Config):
        """
        Initialize the Avellaneda-Stoikov model.
        
        Args:
            config: Configuration object with model parameters
        """
        super().__init__(config)
        
        # Model parameters (configurable via environment)
        self.inventory_risk_aversion = float(self.config.INVENTORY_RISK_AVERSION) if hasattr(self.config, 'INVENTORY_RISK_AVERSION') else 0.1
        self.target_inventory_ratio = float(self.config.TARGET_INVENTORY_RATIO) if hasattr(self.config, 'TARGET_INVENTORY_RATIO') else 0.5
        self.max_inventory_deviation = float(self.config.MAX_INVENTORY_DEVIATION) if hasattr(self.config, 'MAX_INVENTORY_DEVIATION') else 0.3
        self.base_spread = float(self.config.BASE_SPREAD) if hasattr(self.config, 'BASE_SPREAD') else 0.01
        
        # Volatility calculation parameters
        self.volatility_window_size = int(self.config.VOLATILITY_WINDOW_SIZE) if hasattr(self.config, 'VOLATILITY_WINDOW_SIZE') else 20
        self.default_volatility = float(self.config.DEFAULT_VOLATILITY) if hasattr(self.config, 'DEFAULT_VOLATILITY') else 0.02
        self.max_volatility = float(self.config.MAX_VOLATILITY) if hasattr(self.config, 'MAX_VOLATILITY') else 2.0
        self.min_volatility = float(self.config.MIN_VOLATILITY) if hasattr(self.config, 'MIN_VOLATILITY') else 0.01
        
        logger.info(f"Avellaneda-Stoikov model initialized with risk aversion: {self.inventory_risk_aversion}")
    
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
        Calculate optimal LP ranges using Avellaneda-Stoikov model.
        
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
            
            # Calculate base ranges using Avellaneda-Stoikov formula
            range_a, range_b = self._calculate_as_ranges(
                current_ratio, inventory_deviation, volatility, spot_price
            )
            
            # Apply min/max constraints
            range_a = max(range_a, self.config.MIN_RANGE_PERCENTAGE / 100.0)
            range_a = min(range_a, self.config.MAX_RANGE_PERCENTAGE / 100.0)
            range_b = max(range_b, self.config.MIN_RANGE_PERCENTAGE / 100.0)
            range_b = min(range_b, self.config.MAX_RANGE_PERCENTAGE / 100.0)
            
            # Log when constraints are applied
            if range_a == self.config.MIN_RANGE_PERCENTAGE / 100.0 or range_a == self.config.MAX_RANGE_PERCENTAGE / 100.0:
                logger.info(f"Range A constrained to {range_a:.3f} by min/max limits")
            if range_b == self.config.MIN_RANGE_PERCENTAGE / 100.0 or range_b == self.config.MAX_RANGE_PERCENTAGE / 100.0:
                logger.info(f"Range B constrained to {range_b:.3f} by min/max limits")
            
            return {
                'range_a_percentage': range_a * 100.0,  # Convert to percentage
                'range_b_percentage': range_b * 100.0,  # Convert to percentage
                'inventory_ratio': current_ratio,
                'target_ratio': self.target_inventory_ratio,
                'deviation': inventory_deviation,
                'volatility': volatility,
                'model_metadata': {
                    'model_name': self.model_name,
                    'risk_aversion': self.inventory_risk_aversion,
                    'base_spread': self.base_spread,
                    'volatility_window': self.volatility_window_size,
                    'as_formula_used': True
                }
            }
            
        except Exception as e:
            logger.error(f"Error in Avellaneda-Stoikov range calculation: {e}")
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
    
    def _calculate_as_ranges(
        self,
        current_ratio: float,
        inventory_deviation: float,
        volatility: float,
        spot_price: float
    ) -> Tuple[float, float]:
        """
        Calculate ranges using Avellaneda-Stoikov formula.
        
        Args:
            current_ratio: Current inventory ratio
            inventory_deviation: Deviation from target ratio
            volatility: Market volatility
            spot_price: Current spot price
            
        Returns:
            Tuple of (range_a, range_b) as decimals
        """
        try:
            # Avellaneda-Stoikov spread formula
            # Spread = base_spread + (risk_aversion * volatility^2 * inventory_skew)
            
            # Calculate inventory skew (positive if we have too much token0)
            inventory_skew = current_ratio - self.target_inventory_ratio
            
            # Calculate base spread component
            base_spread_component = self.base_spread
            
            # Calculate inventory adjustment component
            inventory_component = self.inventory_risk_aversion * (volatility ** 2) * abs(inventory_skew)
            
            # Total spread
            total_spread = base_spread_component + inventory_component
            
            # Calculate ranges based on inventory position
            if inventory_skew > 0:
                # We have too much token0, make token0 range narrow (encourage selling)
                range_a = total_spread * 0.5  # Narrow range for excess token
                range_b = total_spread * 2.0  # Wide range for deficit token
            else:
                # We have too much token1, make token1 range narrow (encourage selling)
                range_a = total_spread * 2.0  # Wide range for deficit token
                range_b = total_spread * 0.5  # Narrow range for excess token
            
            logger.debug(f"AS ranges calculated: A={range_a:.4f}, B={range_b:.4f}, "
                        f"skew={inventory_skew:.4f}, volatility={volatility:.4f}")
            
            return range_a, range_b
            
        except Exception as e:
            logger.error(f"Error in AS range calculation: {e}")
            # Return default ranges
            return self.base_spread, self.base_spread
    
    def calculate_realized_volatility(self, price_history: List[Dict[str, Any]], window_size: int = 20) -> float:
        """
        Calculate realized volatility using Parkinson estimator.
        
        Args:
            price_history: List of price data points
            window_size: Number of recent prices to use
            
        Returns:
            Realized volatility as a decimal
        """
        try:
            if not price_history or len(price_history) < 2:
                return self.default_volatility
            
            # Use most recent prices
            recent_prices = price_history[-window_size:] if len(price_history) >= window_size else price_history
            
            if len(recent_prices) < 2:
                return self.default_volatility
            
            # Extract high, low, open, close prices if available
            parkinson_estimates = []
            
            for i in range(len(recent_prices)):
                point = recent_prices[i]
                
                # Try to get high/low prices, fallback to open/close
                if 'high' in point and 'low' in point:
                    high = float(point['high'])
                    low = float(point['low'])
                    
                    if high > 0 and low > 0 and high > low:
                        # Parkinson estimator: 0.25 * ln(high/low)^2
                        parkinson_est = 0.25 * (math.log(high / low) ** 2)
                        parkinson_estimates.append(parkinson_est)
                
                elif 'open' in point and 'close' in point:
                    # Fallback to open/close if high/low not available
                    open_price = float(point['open'])
                    close_price = float(point['close'])
                    
                    if open_price > 0 and close_price > 0:
                        # Use absolute return as proxy
                        return_est = abs(math.log(close_price / open_price))
                        parkinson_estimates.append(return_est)
            
            if not parkinson_estimates:
                return self.default_volatility
            
            # Calculate average and annualize
            avg_parkinson = statistics.mean(parkinson_estimates)
            annualized_volatility = math.sqrt(avg_parkinson * 525600)  # 525600 minutes in a year
            
            # Cap volatility at reasonable levels
            annualized_volatility = min(annualized_volatility, self.max_volatility)
            annualized_volatility = max(annualized_volatility, self.min_volatility)
            
            logger.debug(f"Parkinson volatility: {annualized_volatility:.4f} from {len(parkinson_estimates)} estimates")
            return annualized_volatility
            
        except Exception as e:
            logger.error(f"Error calculating Parkinson volatility: {e}")
            return self.default_volatility
    
    def get_model_info(self) -> Dict[str, Any]:
        """
        Get information about the Avellaneda-Stoikov model.
        
        Returns:
            Dictionary with model information
        """
        return {
            'name': 'Avellaneda-Stoikov Model',
            'description': 'Classic market making model adapted for Uniswap V3 LP positions',
            'version': '1.0.0',
            'parameters': {
                'inventory_risk_aversion': self.inventory_risk_aversion,
                'target_inventory_ratio': self.target_inventory_ratio,
                'max_inventory_deviation': self.max_inventory_deviation,
                'base_spread': self.base_spread,
                'volatility_window_size': self.volatility_window_size,
                'default_volatility': self.default_volatility,
                'max_volatility': self.max_volatility,
                'min_volatility': self.min_volatility
            },
            'features': [
                'Dynamic range calculation based on inventory imbalance',
                'Volatility-adjusted spreads',
                'Parkinson volatility estimator',
                'Min/max range constraints',
                'Risk aversion parameter'
            ]
        }
