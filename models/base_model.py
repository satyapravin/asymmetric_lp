"""
Abstract base class for inventory management models.
"""
from abc import ABC, abstractmethod
from typing import Dict, Tuple, List, Any
from config import Config


class BaseInventoryModel(ABC):
    """
    Abstract base class for inventory management models.
    
    All inventory models must implement the calculate_lp_ranges method
    to provide range calculations for LP positions.
    """
    
    def __init__(self, config: Config):
        """
        Initialize the inventory model.
        
        Args:
            config: Configuration object with model parameters
        """
        self.config = config
        self.model_name = self.__class__.__name__
    
    @abstractmethod
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
        Calculate optimal LP ranges based on current inventory and market conditions.
        
        Args:
            token0_balance: Token0 balance in wei
            token1_balance: Token1 balance in wei
            spot_price: Current spot price (token1 per token0)
            price_history: Historical price data
            token_a_address: Token A address (our configured token A)
            token_b_address: Token B address (our configured token B)
            client: Uniswap client for additional data if needed
            
        Returns:
            Dictionary containing:
            - range_a_percentage: Range percentage for position A
            - range_b_percentage: Range percentage for position B
            - inventory_ratio: Current inventory ratio
            - target_ratio: Target inventory ratio
            - deviation: Current deviation from target
            - volatility: Calculated volatility
            - model_metadata: Additional model-specific data
        """
        pass
    
    @abstractmethod
    def get_model_info(self) -> Dict[str, Any]:
        """
        Get information about the model.
        
        Returns:
            Dictionary with model information:
            - name: Model name
            - description: Model description
            - parameters: Model parameters
            - version: Model version
        """
        pass
    
    def validate_inputs(
        self,
        token0_balance: int,
        token1_balance: int,
        spot_price: float,
        price_history: List[Dict[str, Any]]
    ) -> bool:
        """
        Validate input parameters.
        
        Args:
            token0_balance: Token0 balance
            token1_balance: Token1 balance
            spot_price: Current spot price
            price_history: Historical price data
            
        Returns:
            True if inputs are valid, False otherwise
        """
        if token0_balance < 0 or token1_balance < 0:
            return False
        
        if spot_price <= 0:
            return False
        
        if not isinstance(price_history, list):
            return False
        
        return True
    
    def calculate_inventory_ratio(
        self,
        token0_balance: int,
        token1_balance: int,
        spot_price: float,
        token0_decimals: int = 18,
        token1_decimals: int = 18
    ) -> float:
        """
        Calculate current inventory ratio (token0 value / total value).
        
        Args:
            token0_balance: Token0 balance in wei
            token1_balance: Token1 balance in wei
            spot_price: Current spot price
            token0_decimals: Token0 decimals
            token1_decimals: Token1 decimals
            
        Returns:
            Inventory ratio (0.0 to 1.0)
        """
        # Convert balances to human-readable amounts
        token0_amount = token0_balance / (10 ** token0_decimals)
        token1_amount = token1_balance / (10 ** token1_decimals)
        
        # Calculate dollar values
        token0_value = token0_amount * spot_price  # token0 value in token1 terms
        token1_value = token1_amount  # token1 value in token1 terms
        
        total_value = token0_value + token1_value
        
        if total_value == 0:
            return 0.5  # Default to balanced if no value
        
        return token0_value / total_value
    
    def calculate_volatility(self, price_history: List[Dict[str, Any]], window_size: int = 20) -> float:
        """
        Basic volatility calculation using log returns.
        
        Args:
            price_history: Historical price data
            window_size: Number of recent prices to use
            
        Returns:
            Volatility as a decimal
        """
        import math
        import statistics
        
        if not price_history or len(price_history) < 2:
            return 0.02  # Default 2% volatility
        
        # Use most recent prices
        recent_prices = price_history[-window_size:] if len(price_history) >= window_size else price_history
        
        if len(recent_prices) < 2:
            return 0.02
        
        # Extract prices and calculate log returns
        prices = [float(point['price']) for point in recent_prices]
        log_returns = []
        
        for i in range(1, len(prices)):
            if prices[i-1] > 0:
                log_return = math.log(prices[i] / prices[i-1])
                log_returns.append(log_return)
        
        if len(log_returns) < 2:
            return 0.02
        
        # Calculate and annualize volatility
        volatility = statistics.stdev(log_returns)
        annualized_volatility = volatility * math.sqrt(525600)  # 525600 minutes in a year
        
        # Cap volatility at reasonable levels
        return max(0.01, min(annualized_volatility, 2.0))
