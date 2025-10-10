"""
Inventory management model for calculating LP ranges.
Uses Avellaneda-Stoikov theory adapted for Uniswap V3.
"""
import math
import logging
import statistics
from typing import Dict, Tuple, Optional, List
from decimal import Decimal, getcontext
from config import Config

# Set high precision for calculations
getcontext().prec = 50

logger = logging.getLogger(__name__)

class InventoryModel:
    """
    Inventory management model for dynamic LP range calculation
    Based on Avellaneda-Stoikov market making theory
    """
    
    def __init__(self, config: Config):
        self.config = config
        
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
        
        logger.info(f"Inventory model initialized with risk aversion: {self.inventory_risk_aversion}")
    
    def calculate_volatility(self, price_history: List[Dict], window_size: int = 20) -> float:
        """
        Calculate price volatility from historical price data
        
        Args:
            price_history: List of price data points with 'price' field
            window_size: Number of recent prices to use for volatility calculation
            
        Returns:
            Volatility as a decimal (e.g., 0.02 for 2% volatility)
        """
        try:
            if not price_history or len(price_history) < 2:
                return self.default_volatility  # Default volatility if insufficient data
            
            # Use most recent prices
            recent_prices = price_history[-window_size:] if len(price_history) >= window_size else price_history
            
            if len(recent_prices) < 2:
                return self.default_volatility
            
            # Extract prices
            prices = [float(point['price']) for point in recent_prices]
            
            # Calculate log returns
            log_returns = []
            for i in range(1, len(prices)):
                if prices[i-1] > 0:  # Avoid division by zero
                    log_return = math.log(prices[i] / prices[i-1])
                    log_returns.append(log_return)
            
            if not log_returns:
                return self.default_volatility
            
            # Calculate standard deviation of log returns
            volatility = statistics.stdev(log_returns)
            
            # Annualize volatility (assuming 1-minute intervals)
            # For different time intervals, adjust the multiplier
            annualized_volatility = volatility * math.sqrt(525600)  # 525600 minutes in a year
            
            # Cap volatility at reasonable levels
            annualized_volatility = min(annualized_volatility, self.max_volatility)
            annualized_volatility = max(annualized_volatility, self.min_volatility)
            
            logger.debug(f"Calculated volatility: {annualized_volatility:.4f} from {len(log_returns)} price points")
            return annualized_volatility
            
        except Exception as e:
            logger.error(f"Error calculating volatility: {e}")
            return self.default_volatility  # Default volatility on error
    
    def calculate_realized_volatility(self, price_history: List[Dict], window_size: int = 20) -> float:
        """
        Calculate realized volatility using Parkinson estimator (more efficient for high-frequency data)
        
        Args:
            price_history: List of price data points
            window_size: Number of recent prices to use
            
        Returns:
            Realized volatility as a decimal
        """
        try:
            if not price_history or len(price_history) < 2:
                return 0.02
            
            recent_prices = price_history[-window_size:] if len(price_history) >= window_size else price_history
            
            if len(recent_prices) < 2:
                return self.default_volatility
            
            prices = [float(point['price']) for point in recent_prices]
            
            # Calculate Parkinson volatility estimator
            parkinson_sum = 0
            for i in range(1, len(prices)):
                if prices[i-1] > 0:
                    # Parkinson estimator: 0.25 * ln(high/low)^2
                    high_low_ratio = prices[i] / prices[i-1]
                    parkinson_sum += 0.25 * (math.log(high_low_ratio) ** 2)
            
            if len(prices) <= 1:
                return 0.02
            
            # Calculate average and annualize
            avg_parkinson = parkinson_sum / (len(prices) - 1)
            realized_volatility = math.sqrt(avg_parkinson * 525600)  # Annualize
            
            # Cap volatility
            realized_volatility = min(realized_volatility, 2.0)
            realized_volatility = max(realized_volatility, 0.01)
            
            logger.debug(f"Calculated realized volatility: {realized_volatility:.4f}")
            return realized_volatility
            
        except Exception as e:
            logger.error(f"Error calculating realized volatility: {e}")
            return 0.02
    
    def calculate_token_values(self, token_a_balance: int, token_b_balance: int, 
                             spot_price: float, token_a_address: str = None,
                             token_b_address: str = None, client = None) -> Dict[str, float]:
        """
        Calculate dollar values of token balances with dynamic decimals
        
        Args:
            token_a_balance: Balance of token A (in wei)
            token_b_balance: Balance of token B (in wei)
            spot_price: Current spot price (token B per token A)
            token_a_address: Token A contract address (for dynamic decimals)
            token_b_address: Token B contract address (for dynamic decimals)
            client: UniswapV3Client instance for fetching decimals
            
        Returns:
            Dictionary with token values and ratios
        """
        # Get token decimals dynamically if client is provided
        if client and token_a_address and token_b_address:
            token_a_decimals = client.get_token_decimals(token_a_address)
            token_b_decimals = client.get_token_decimals(token_b_address)
        else:
            # Fallback to default decimals
            token_a_decimals = 18
            token_b_decimals = 18
        # Convert wei to token units
        token_a_amount = token_a_balance / (10 ** token_a_decimals)
        token_b_amount = token_b_balance / (10 ** token_b_decimals)
        
        # Calculate dollar values using spot price
        # spot_price represents token1 per token0 (from Uniswap)
        # We need to map our token_a/token_b to the actual token0/token1 ordering
        
        # Determine which token is token0 and which is token1
        # In Uniswap, token0 has the lower address value
        if token_a_address and token_b_address:
            token_a_is_token0 = token_a_address.lower() < token_b_address.lower()
        else:
            # Fallback: assume token_a is token0
            token_a_is_token0 = True
        
        logger.debug(f"Token ordering:")
        logger.debug(f"  Token A: {token_a_address} (decimals: {token_a_decimals})")
        logger.debug(f"  Token B: {token_b_address} (decimals: {token_b_decimals})")
        logger.debug(f"  Token A is token0: {token_a_is_token0}")
        logger.debug(f"  Spot price: {spot_price:.10f} (token1/token0)")
        
        if token_a_is_token0:
            # token_a = token0, token_b = token1
            # spot_price = token1/token0 = token_b/token_a
            token_a_value_usd = token_a_amount * spot_price  # Convert token_a to token_b units
            token_b_value_usd = token_b_amount  # token_b in its own units
            logger.debug(f"  Value calculation: token_a * spot_price = {token_a_value_usd:.6f}")
        else:
            # token_a = token1, token_b = token0  
            # spot_price = token1/token0 = token_a/token_b
            token_a_value_usd = token_a_amount  # token_a in its own units
            token_b_value_usd = token_b_amount / spot_price  # Convert token_b to token_a units
            logger.debug(f"  Value calculation: token_b / spot_price = {token_b_value_usd:.6f}")
        
        total_value = token_a_value_usd + token_b_value_usd
        
        # Calculate ratios
        token_a_ratio = token_a_value_usd / total_value if total_value > 0 else 0
        token_b_ratio = token_b_value_usd / total_value if total_value > 0 else 0
        
        return {
            'token_a_amount': token_a_amount,
            'token_b_amount': token_b_amount,
            'token_a_value_usd': token_a_value_usd,
            'token_b_value_usd': token_b_value_usd,
            'total_value_usd': total_value,
            'token_a_ratio': token_a_ratio,
            'token_b_ratio': token_b_ratio,
            'inventory_imbalance': abs(token_a_ratio - self.target_inventory_ratio)
        }
    
    def calculate_optimal_spreads(self, inventory_ratio: float, spot_price: float, 
                                price_history: List[Dict] = None) -> Tuple[float, float]:
        """
        Calculate optimal bid-ask spreads based on inventory and volatility
        
        Args:
            inventory_ratio: Current ratio of token A to total value
            spot_price: Current spot price
            price_history: Historical price data for volatility calculation
            
        Returns:
            Tuple of (bid_spread, ask_spread) as percentages
        """
        # Calculate volatility from price history
        if price_history and len(price_history) > 1:
            volatility = self.calculate_realized_volatility(price_history)
        else:
            volatility = 0.02  # Default 2% volatility
        
        # Inventory deviation from target
        inventory_deviation = inventory_ratio - self.target_inventory_ratio
        
        # Avellaneda-Stoikov spread calculation
        # Spread increases with inventory deviation and volatility
        base_spread = self.base_spread
        
        # Inventory adjustment factor
        inventory_adjustment = self.inventory_risk_aversion * abs(inventory_deviation)
        
        # Volatility adjustment
        volatility_adjustment = volatility * math.sqrt(2 * math.log(2))
        
        # Calculate spreads
        if inventory_deviation > 0:
            # Excess token A - wider ask spread, narrower bid spread
            ask_spread = base_spread + inventory_adjustment + volatility_adjustment
            bid_spread = base_spread + volatility_adjustment
        else:
            # Excess token B - wider bid spread, narrower ask spread
            bid_spread = base_spread + inventory_adjustment + volatility_adjustment
            ask_spread = base_spread + volatility_adjustment
        
        # Ensure minimum spreads
        bid_spread = max(bid_spread, self.config.MIN_RANGE_PERCENTAGE / 100)
        ask_spread = max(ask_spread, self.config.MIN_RANGE_PERCENTAGE / 100)
        
        # Ensure maximum spreads
        bid_spread = min(bid_spread, self.config.MAX_RANGE_PERCENTAGE / 100)
        ask_spread = min(ask_spread, self.config.MAX_RANGE_PERCENTAGE / 100)
        
        logger.debug(f"Spread calculation: volatility={volatility:.4f}, inventory_deviation={inventory_deviation:.4f}")
        logger.debug(f"Calculated spreads: bid={bid_spread:.4f}, ask={ask_spread:.4f}")
        
        return bid_spread, ask_spread
    
    def calculate_lp_ranges(self, token_a_balance: int, token_b_balance: int,
                           spot_price: float, price_history: List[Dict] = None,
                           token_a_address: str = None, token_b_address: str = None,
                           client = None) -> Dict[str, float]:
        """
        Calculate optimal LP ranges based on inventory management model
        
        Args:
            token_a_balance: Balance of token A (in wei)
            token_b_balance: Balance of token B (in wei)
            spot_price: Current spot price
            price_history: Historical price data for volatility calculation
            token_a_address: Token A contract address (for dynamic decimals)
            token_b_address: Token B contract address (for dynamic decimals)
            client: UniswapV3Client instance for fetching decimals
            
        Returns:
            Dictionary with calculated ranges and positions
        """
        # Calculate token values and ratios with dynamic decimals
        token_values = self.calculate_token_values(
            token_a_balance, token_b_balance, spot_price, 
            token_a_address, token_b_address, client
        )
        
        # Calculate optimal spreads using price history for volatility
        bid_spread, ask_spread = self.calculate_optimal_spreads(
            token_values['token_a_ratio'], spot_price, price_history
        )
        
        # Determine which token has excess inventory
        token_a_ratio = token_values['token_a_ratio']
        token_b_ratio = token_values['token_b_ratio']
        
        if token_a_ratio > self.target_inventory_ratio:
            # Excess token A - narrow range for token A position, wide range for token B position
            token_a_range_percent = bid_spread * 100
            token_b_range_percent = ask_spread * 100
            excess_token = 'A'
        else:
            # Excess token B - wide range for token A position, narrow range for token B position
            token_a_range_percent = ask_spread * 100
            token_b_range_percent = bid_spread * 100
            excess_token = 'B'
        
        # Ensure final ranges conform to MIN_RANGE_PERCENTAGE and MAX_RANGE_PERCENTAGE
        original_a_range = token_a_range_percent
        original_b_range = token_b_range_percent
        
        token_a_range_percent = max(token_a_range_percent, self.config.MIN_RANGE_PERCENTAGE)
        token_a_range_percent = min(token_a_range_percent, self.config.MAX_RANGE_PERCENTAGE)
        
        token_b_range_percent = max(token_b_range_percent, self.config.MIN_RANGE_PERCENTAGE)
        token_b_range_percent = min(token_b_range_percent, self.config.MAX_RANGE_PERCENTAGE)
        
        # Log if constraints were applied
        if original_a_range != token_a_range_percent:
            logger.debug(f"Token A range constrained: {original_a_range:.2f}% -> {token_a_range_percent:.2f}%")
        if original_b_range != token_b_range_percent:
            logger.debug(f"Token B range constrained: {original_b_range:.2f}% -> {token_b_range_percent:.2f}%")
        
        # Calculate price ranges
        token_a_lower_price = spot_price * (1 - token_a_range_percent / 100)
        token_a_upper_price = spot_price * (1 + token_a_range_percent / 100)
        
        token_b_lower_price = spot_price * (1 - token_b_range_percent / 100)
        token_b_upper_price = spot_price * (1 + token_b_range_percent / 100)
        
        # Calculate position sizes based on inventory
        if excess_token == 'A':
            # Use more token A in the narrow range position
            token_a_position_size = min(token_values['token_a_amount'] * 0.8, token_values['total_value_usd'] * 0.6)
            token_b_position_size = min(token_values['token_b_amount'] * 0.3, token_values['total_value_usd'] * 0.4)
        else:
            # Use more token B in the narrow range position
            token_a_position_size = min(token_values['token_a_amount'] * 0.3, token_values['total_value_usd'] * 0.4)
            token_b_position_size = min(token_values['token_b_amount'] * 0.8, token_values['total_value_usd'] * 0.6)
        
        result = {
            'spot_price': spot_price,
            'token_a_range_percent': token_a_range_percent,
            'token_b_range_percent': token_b_range_percent,
            'token_a_lower_price': token_a_lower_price,
            'token_a_upper_price': token_a_upper_price,
            'token_b_lower_price': token_b_lower_price,
            'token_b_upper_price': token_b_upper_price,
            'token_a_position_size': token_a_position_size,
            'token_b_position_size': token_b_position_size,
            'excess_token': excess_token,
            'inventory_imbalance': token_values['inventory_imbalance'],
            'target_rebalance': token_values['inventory_imbalance'] > self.max_inventory_deviation,
            'token_values': token_values
        }
        
        logger.info(f"Inventory model calculation:")
        logger.info(f"  Token A ratio: {token_a_ratio:.3f}, Token B ratio: {token_b_ratio:.3f}")
        logger.info(f"  Excess token: {excess_token}")
        logger.info(f"  AS spreads: bid={bid_spread*100:.2f}%, ask={ask_spread*100:.2f}%")
        logger.info(f"  Final ranges: Token A={token_a_range_percent:.2f}%, Token B={token_b_range_percent:.2f}%")
        logger.info(f"  Range constraints: MIN={self.config.MIN_RANGE_PERCENTAGE}%, MAX={self.config.MAX_RANGE_PERCENTAGE}%")
        logger.info(f"  Inventory imbalance: {token_values['inventory_imbalance']:.3f}")
        logger.info(f"  Target rebalance: {result['target_rebalance']}")
        
        return result
    
    def should_rebalance(self, inventory_imbalance: float) -> bool:
        """
        Determine if rebalancing is needed based on inventory imbalance
        
        Args:
            inventory_imbalance: Current inventory imbalance ratio
            
        Returns:
            True if rebalancing is recommended
        """
        return inventory_imbalance > self.max_inventory_deviation
    
    def get_inventory_adjustment_factor(self, current_ratio: float) -> float:
        """
        Calculate inventory adjustment factor for position sizing
        
        Args:
            current_ratio: Current inventory ratio
            
        Returns:
            Adjustment factor (0.0 to 1.0)
        """
        deviation = abs(current_ratio - self.target_inventory_ratio)
        return min(deviation / self.max_inventory_deviation, 1.0)
