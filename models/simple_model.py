"""
Simple inventory management model example.
This demonstrates how to create a new model that inherits from BaseInventoryModel.
"""
import logging
from typing import Dict, List, Any
from config import Config
from .base_model import BaseInventoryModel

logger = logging.getLogger(__name__)


class SimpleModel(BaseInventoryModel):
    """
    Simple inventory management model.
    
    This is a basic example that uses fixed ranges with simple inventory adjustment.
    It's much simpler than Avellaneda-Stoikov but demonstrates the interface.
    """
    
    def __init__(self, config: Config):
        """
        Initialize the simple model.
        
        Args:
            config: Configuration object
        """
        super().__init__(config)
        
        # Simple model parameters
        self.base_range = 0.05  # 5% base range
        self.max_range = 0.20   # 20% maximum range
        self.min_range = 0.02   # 2% minimum range
        self.inventory_sensitivity = 2.0  # How sensitive to inventory imbalance
        
        logger.info("Simple model initialized")
    
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
        Calculate ranges using simple inventory-based logic.
        
        Args:
            token0_balance: Token0 balance in wei
            token1_balance: Token1 balance in wei
            spot_price: Current spot price
            price_history: Historical price data
            token_a_address: Token A address
            token_b_address: Token B address
            client: Uniswap client
            
        Returns:
            Dictionary containing range calculations
        """
        try:
            # Validate inputs
            if not self.validate_inputs(token0_balance, token1_balance, spot_price, price_history):
                raise ValueError("Invalid input parameters")
            
            # Get token decimals
            token0_decimals = client.get_token_decimals(token_a_address)
            token1_decimals = client.get_token_decimals(token_b_address)
            
            # Calculate current inventory ratio
            current_ratio = self.calculate_inventory_ratio(
                token0_balance, token1_balance, spot_price, token0_decimals, token1_decimals
            )
            
            # Calculate inventory deviation from target (50/50)
            target_ratio = 0.5
            inventory_deviation = abs(current_ratio - target_ratio)
            
            # Simple range calculation
            if current_ratio > target_ratio:
                # We have too much token0, make token0 range narrow
                range_a = self.base_range * (1 - inventory_deviation * self.inventory_sensitivity)
                range_b = self.base_range * (1 + inventory_deviation * self.inventory_sensitivity)
            else:
                # We have too much token1, make token1 range narrow
                range_a = self.base_range * (1 + inventory_deviation * self.inventory_sensitivity)
                range_b = self.base_range * (1 - inventory_deviation * self.inventory_sensitivity)
            
            # Apply min/max constraints
            range_a = max(self.min_range, min(range_a, self.max_range))
            range_b = max(self.min_range, min(range_b, self.max_range))
            
            # Calculate volatility (using base class method)
            volatility = self.calculate_volatility(price_history)
            
            return {
                'range_a_percentage': range_a * 100.0,
                'range_b_percentage': range_b * 100.0,
                'inventory_ratio': current_ratio,
                'target_ratio': target_ratio,
                'deviation': inventory_deviation,
                'volatility': volatility,
                'model_metadata': {
                    'model_name': self.model_name,
                    'base_range': self.base_range,
                    'inventory_sensitivity': self.inventory_sensitivity,
                    'simple_logic_used': True
                }
            }
            
        except Exception as e:
            logger.error(f"Error in simple model calculation: {e}")
            # Return default ranges on error
            return {
                'range_a_percentage': self.min_range * 100.0,
                'range_b_percentage': self.min_range * 100.0,
                'inventory_ratio': 0.5,
                'target_ratio': 0.5,
                'deviation': 0.0,
                'volatility': 0.02,
                'model_metadata': {
                    'model_name': self.model_name,
                    'error': str(e),
                    'fallback_used': True
                }
            }
    
    def get_model_info(self) -> Dict[str, Any]:
        """
        Get information about the simple model.
        
        Returns:
            Dictionary with model information
        """
        return {
            'name': 'Simple Model',
            'description': 'Basic inventory-based range calculation with fixed parameters',
            'version': '1.0.0',
            'parameters': {
                'base_range': self.base_range,
                'max_range': self.max_range,
                'min_range': self.min_range,
                'inventory_sensitivity': self.inventory_sensitivity
            },
            'features': [
                'Simple inventory-based range adjustment',
                'Fixed base range with inventory scaling',
                'Min/max range constraints',
                'No volatility consideration',
                'Easy to understand and modify'
            ]
        }
