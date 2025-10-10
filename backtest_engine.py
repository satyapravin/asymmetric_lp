#!/usr/bin/env python3
"""
AsymmetricLP - Backtesting Module
Simulates asymmetric LP rebalancing using historical OHLC data
"""
import logging
import pandas as pd
import numpy as np
from typing import Dict, List, Tuple, Any, Optional
from datetime import datetime, timedelta
import json
from dataclasses import dataclass
from config import Config
from models.model_factory import ModelFactory
from alert_manager import TelegramAlertManager
from inventory_publisher import InventoryPublisher

logger = logging.getLogger(__name__)

@dataclass
class BacktestPosition:
    """Represents a simulated LP position"""
    token_id: str
    token0_amount: float
    token1_amount: float
    tick_lower: int
    tick_upper: int
    liquidity: float
    fees_collected_0: float = 0.0
    fees_collected_1: float = 0.0
    created_at: datetime = None

@dataclass
class BacktestTrade:
    """Represents a simulated trade"""
    timestamp: datetime
    price: float
    volume: float
    trade_type: str  # 'buy' or 'sell'
    fees_paid: float

@dataclass
class BacktestResult:
    """Results from a backtest run"""
    start_time: datetime
    end_time: datetime
    initial_balance_0: float
    initial_balance_1: float
    final_balance_0: float
    final_balance_1: float
    total_fees_collected: float
    total_rebalances: int
    total_trades: int
    max_drawdown: float
    sharpe_ratio: float
    win_rate: float
    trades: List[BacktestTrade]
    rebalances: List[Dict[str, Any]]

class BacktestEngine:
    """Engine for backtesting LP rebalancing strategies"""
    
    def __init__(self, config: Config):
        """
        Initialize backtest engine
        
        Args:
            config: Configuration object
        """
        self.config = config
        # Initialize inventory model using factory
        model_name = getattr(config, 'INVENTORY_MODEL', 'AvellanedaStoikovModel')
        self.inventory_model = ModelFactory.create_model(model_name, config)
        
        # Disable external services for backtesting
        self.alert_manager = None
        self.inventory_publisher = None
        
        # Backtest state
        self.positions: List[BacktestPosition] = []
        self.balance_0 = 0.0  # Token A balance
        self.balance_1 = 0.0  # Token B balance
        self.price_history = []
        self.trades = []
        self.rebalances = []
        self.last_rebalance_time = None
        
        # Performance tracking
        self.portfolio_values = []  # Track portfolio value over time
        self.total_fees_collected_0 = 0.0  # Track total fees collected
        self.total_fees_collected_1 = 0.0  # Track total fees collected
        
        # Fee tier in basis points (e.g., 3000 = 0.3%)
        self.fee_tier_bps = config.FEE_TIER
        
        logger.info("Backtest engine initialized")
    
    def load_ohlc_data(self, file_path: str) -> pd.DataFrame:
        """
        Load OHLC data from CSV file
        
        Expected format:
        timestamp,open,high,low,close,volume
        
        Args:
            file_path: Path to CSV file
            
        Returns:
            DataFrame with OHLC data
        """
        try:
            df = pd.read_csv(file_path)
            
            # Ensure required columns exist
            required_cols = ['timestamp', 'open', 'high', 'low', 'close', 'volume']
            if not all(col in df.columns for col in required_cols):
                raise ValueError(f"CSV must contain columns: {required_cols}")
            
            # Convert timestamp to datetime
            df['timestamp'] = pd.to_datetime(df['timestamp'])
            
            # Sort by timestamp
            df = df.sort_values('timestamp').reset_index(drop=True)
            
            logger.info(f"Loaded {len(df)} OHLC records from {file_path}")
            logger.info(f"Date range: {df['timestamp'].min()} to {df['timestamp'].max()}")
            
            return df
            
        except Exception as e:
            logger.error(f"Error loading OHLC data: {e}")
            raise
    
    def detect_trades_from_ohlc(self, df: pd.DataFrame) -> List[BacktestTrade]:
        """
        Detect trades from OHLC data based on price movements
        
        A trade is detected if:
        - Price moves more than the fee tier (indicating a swap occurred)
        - Volume is above a minimum threshold
        
        Args:
            df: DataFrame with OHLC data
            
        Returns:
            List of detected trades
        """
        trades = []
        fee_threshold = self.fee_tier_bps / 10000  # Convert to decimal (3000 bps = 0.3%)
        
        # Debug: Check fee tier calculation
        logger.debug(f"Fee tier: {self.fee_tier_bps} bps = {fee_threshold:.4%}")
        
        for i, row in df.iterrows():
            timestamp = row['timestamp']
            open_price = row['open']
            high_price = row['high']
            low_price = row['low']
            close_price = row['close']
            volume = row['volume']
            
            # Skip if no volume
            if volume <= 0:
                continue
            
            # Calculate price movements
            high_move = (high_price - open_price) / open_price
            low_move = (open_price - low_price) / open_price
            close_move = abs(close_price - open_price) / open_price
            
            # Use a realistic trade detection threshold for backtesting
            # In reality, many small trades happen that don't exceed the fee tier
            # Use 0.05% threshold for realistic trade detection (vs 0.3% fee tier)
            trade_threshold = 0.0005  # 0.05% threshold for backtesting
            
            # Detect trades based on significant price movements
            # Only one trade per minute if price movement exceeds fee tier
            max_move = max(high_move, low_move, close_move)
            
            if max_move > trade_threshold:
                # Determine trade direction based on which movement was largest
                if high_move == max_move:
                    # Price went up significantly - likely buy pressure
                    trade_price = high_price
                    trade_type = 'buy'
                    trade_volume = volume * 0.5  # Assume 50% of volume at high
                elif low_move == max_move:
                    # Price went down significantly - likely sell pressure
                    trade_price = low_price
                    trade_type = 'sell'
                    trade_volume = volume * 0.5  # Assume 50% of volume at low
                else:
                    # Close movement was largest
                    trade_price = close_price
                    trade_type = 'buy' if close_price > open_price else 'sell'
                    trade_volume = volume * 0.3  # Assume 30% of volume at close
                
                # Calculate fee based on price movement, not volume
                # Fee will be calculated in simulate_lp_fees based on position size and price movement
                trades.append(BacktestTrade(
                    timestamp=timestamp,
                    price=trade_price,
                    volume=0.0,  # Ignore volume - we'll calculate based on position size
                    trade_type=trade_type,
                    fees_paid=0.0  # Will be calculated in simulate_lp_fees
                ))
        
        logger.info(f"Detected {len(trades)} trades from OHLC data")
        return trades
    
    def simulate_lp_fees(self, trades: List[BacktestTrade], current_price: float) -> Tuple[float, float]:
        """
        Simulate LP fee collection from trades based on position ranges
        
        Args:
            trades: List of trades in current period
            current_price: Current price
            
        Returns:
            Tuple of (fees_collected_0, fees_collected_1)
        """
        total_fees_0 = 0.0
        total_fees_1 = 0.0
        
        for trade in trades:
            # Simulate Uniswap V3 swap mechanics
            # Only positions with ranges that include the trade price participate
            active_positions = []
            total_active_liquidity = 0.0
            
            logger.info(f"Processing trade: price={trade.price:.2f}, volume={trade.volume:.2f}, type={trade.trade_type}, fees_paid={trade.fees_paid:.6f}")
            logger.info(f"Current positions: {len(self.positions)}")
            
            # Find positions that are active for this trade price
            for position in self.positions:
                if position.tick_lower <= trade.price <= position.tick_upper:
                    # Position is active - calculate its liquidity at this price
                    position_liquidity = self._calculate_position_liquidity_at_price(position, trade.price)
                    if position_liquidity > 0:
                        active_positions.append((position, position_liquidity))
                        total_active_liquidity += position_liquidity
                        logger.debug(f"Active position: price={trade.price:.2f}, range=[{position.tick_lower:.2f}, {position.tick_upper:.2f}], liquidity={position_liquidity:.2f}")
            
            # Calculate fees based on actual LP mechanics: price movement relative to position range
            if active_positions and total_active_liquidity > 0:
                for position, position_liquidity in active_positions:
                    # Calculate position range width
                    range_width = position.tick_upper - position.tick_lower
                    
                    if range_width > 0:
                        # Calculate how much of the position gets filled by this price movement
                        # For simplicity, assume each trade moves price by a small amount within the range
                        # This simulates the cumulative effect of many small trades
                        price_movement_ratio = 0.001  # 0.1% of range per trade (realistic for minute data)
                        
                        # Calculate position value in USD
                        position_value_usd = (position.token0_amount * current_price) + position.token1_amount
                        
                        # Calculate filled amount based on price movement relative to range
                        fill_ratio = price_movement_ratio
                        filled_amount_usd = position_value_usd * fill_ratio
                        
                        # Calculate fee based on filled amount
                        fee_amount_usd = filled_amount_usd * self.fee_tier_bps / 10000  # Apply fee rate
                        
                        # Distribute fee between token0 and token1 based on position composition
                        if position.token0_amount > 0 and position.token1_amount > 0:
                            # Mixed position - split fee proportionally
                            token0_ratio = (position.token0_amount * current_price) / position_value_usd
                            token1_ratio = position.token1_amount / position_value_usd
                            
                            fees_0 = (fee_amount_usd / current_price) * token0_ratio  # Convert to token0
                            fees_1 = fee_amount_usd * token1_ratio  # Keep in USD (token1)
                        elif position.token0_amount > 0:
                            # Token0 only position
                            fees_0 = fee_amount_usd / current_price  # Convert to token0
                            fees_1 = 0.0
                        else:
                            # Token1 only position
                            fees_0 = 0.0
                            fees_1 = fee_amount_usd  # Keep in USD (token1)
                        
                        # Add fees to position
                        position.fees_collected_0 += fees_0
                        position.fees_collected_1 += fees_1
                        
                        total_fees_0 += fees_0
                        total_fees_1 += fees_1
                        
                        logger.debug(f"LP Fee: range_width={range_width:.4f}, fill_ratio={fill_ratio:.4f}, position_value=${position_value_usd:.2f}, filled=${filled_amount_usd:.2f}, fee=${fee_amount_usd:.2f}")
        
        return total_fees_0, total_fees_1
    
    def _calculate_position_liquidity_at_price(self, position: BacktestPosition, trade_price: float) -> float:
        """
        Calculate how much liquidity a position has at a specific price
        
        In Uniswap V3, a position's liquidity is distributed across its price range.
        At any given price, only a portion of the position's total liquidity is active.
        
        Args:
            position: LP position
            trade_price: Price at which to calculate liquidity
            
        Returns:
            Active liquidity at the given price
        """
        # For simplicity, we'll use the position's total liquidity
        # In a more sophisticated implementation, we'd calculate the actual
        # liquidity distribution across the price range
        
        # Check if price is within position range
        if position.tick_lower <= trade_price <= position.tick_upper:
            # Position is active at this price
            # Use the position's liquidity as a proxy for active liquidity
            return position.liquidity
        else:
            # Position is not active at this price
            return 0.0
    
    def _calculate_position_liquidity_share(self, position: BacktestPosition, current_price: float) -> float:
        """
        Calculate this position's share of total liquidity for fee distribution
        
        Args:
            position: LP position
            current_price: Current price
            
        Returns:
            Position's liquidity share (0.0 to 1.0)
        """
        if not self.positions:
            return 0.0
        
        # Calculate total liquidity across all positions
        total_liquidity = 0.0
        for pos in self.positions:
            total_liquidity += pos.liquidity
        
        if total_liquidity == 0:
            return 0.0
        
        # Return this position's share
        return position.liquidity / total_liquidity
    
    def calculate_position_value(self, position: BacktestPosition, current_price: float) -> Tuple[float, float]:
        """
        Calculate current value of a position
        
        Args:
            position: LP position
            current_price: Current price
            
        Returns:
            Tuple of (value_0, value_1)
        """
        # Range-based position valuation with gradual falloff
        # In Uniswap V3, position value depends on current price vs range
        # Use gradual falloff instead of binary on/off
        
        # Calculate how far price is from range center
        range_center = (position.tick_upper + position.tick_lower) / 2
        range_width = position.tick_upper - position.tick_lower
        
        if range_width > 0:
            # Calculate distance from range center as fraction of range width
            distance_from_center = abs(current_price - range_center) / (range_width / 2)
            # Apply gradual falloff (positions lose value gradually as price moves away)
            falloff_factor = max(0.1, 1.0 - distance_from_center * 0.5)  # Minimum 10% value
        else:
            falloff_factor = 1.0
        
        # Apply falloff to position values
        # Convert both token amounts to USD for consistent units
        # Temporarily disable falloff factor to test
        value_0 = position.token0_amount * current_price  # Convert BTC to USD (no falloff)
        value_1 = position.token1_amount                  # USDC is already in USD (no falloff)
        
        # Don't add fees to position value - fees are collected separately
        # This prevents unrealistic compounding effects in backtesting
        
        return value_0, value_1
    
    def calculate_portfolio_value(self, current_price: float) -> float:
        """
        Calculate total portfolio value at current price
        
        Args:
            current_price: Current price
            
        Returns:
            Total portfolio value in USD
        """
        # Calculate balance values in USD
        # token0 is BTC (priced in USD), token1 is USDC (already in USD)
        balance_value_0 = self.balance_0 * current_price  # Convert BTC to USD
        balance_value_1 = self.balance_1                   # USDC is already in USD
        
        # Add position values
        position_value_0 = 0.0
        position_value_1 = 0.0
        
        for position in self.positions:
            pos_value_0, pos_value_1 = self.calculate_position_value(position, current_price)
            position_value_0 += pos_value_0
            position_value_1 += pos_value_1
            
            # Don't add fees here - they're tracked separately to prevent compounding
        
        # Add separately tracked fees to portfolio value
        fees_value_0 = self.total_fees_collected_0 * current_price  # Convert BTC fees to USD
        fees_value_1 = self.total_fees_collected_1                  # USDC fees already in USD
        
        total_value = balance_value_0 + balance_value_1 + position_value_0 + position_value_1 + fees_value_0 + fees_value_1
        return total_value
    
    def calculate_sharpe_ratio(self, risk_free_rate: float = 0.02) -> float:
        """
        Calculate Sharpe ratio from portfolio values
        
        Args:
            risk_free_rate: Annual risk-free rate (default 2%)
            
        Returns:
            Sharpe ratio
        """
        if len(self.portfolio_values) < 2:
            return 0.0
        
        # Convert to numpy array
        values = np.array(self.portfolio_values)
        
        # Calculate total return over the period
        total_return = (values[-1] - values[0]) / values[0]
        
        # Calculate period length in days
        period_days = len(values) / (24 * 60)  # Convert minutes to days
        
        # For short periods, use a more conservative approach
        if period_days < 30:
            # If total return is very small, return 0 (not meaningful)
            if abs(total_return) < 0.005:  # Less than 0.5% total return
                return 0.0
            
            # Use total return divided by period length as daily return estimate
            estimated_daily_return = total_return / period_days
            
            # Use a fixed minimum daily volatility for short periods
            # This prevents artificially high Sharpe ratios from near-zero volatility
            estimated_daily_volatility = 0.02  # 2% daily volatility (conservative)
            
            # Calculate Sharpe ratio (don't annualize for short periods)
            sharpe_ratio = estimated_daily_return / estimated_daily_volatility
        else:
            # For longer periods, use daily returns
            minutes_per_day = 1440
            daily_returns = []
            
            for i in range(0, len(values) - 1, minutes_per_day):
                if i + minutes_per_day < len(values):
                    daily_return = (values[i + minutes_per_day] - values[i]) / values[i]
                    daily_returns.append(daily_return)
            
            if len(daily_returns) < 2:
                return 0.0
            
            daily_returns = np.array(daily_returns)
            avg_daily_return = np.mean(daily_returns)
            daily_volatility = np.std(daily_returns)
            
            if daily_volatility == 0:
                return 0.0
            
            sharpe_ratio = (avg_daily_return / daily_volatility) * np.sqrt(365)
        
        return sharpe_ratio
    
    def calculate_max_drawdown(self) -> float:
        """
        Calculate maximum drawdown from portfolio values
        
        Returns:
            Maximum drawdown as a percentage
        """
        if len(self.portfolio_values) < 2:
            return 0.0
        
        # Convert to numpy array
        values = np.array(self.portfolio_values)
        
        # Calculate running maximum
        running_max = np.maximum.accumulate(values)
        
        # Calculate drawdown
        drawdown = (values - running_max) / running_max
        
        # Return maximum drawdown as positive percentage
        max_drawdown = abs(np.min(drawdown))
        
        return max_drawdown
    
    def should_rebalance(self, current_price: float) -> bool:
        """
        Determine if rebalancing is needed based on inventory model
        
        Args:
            current_price: Current price
            
        Returns:
            True if rebalancing is needed
        """
        if not self.price_history:
            return False
        
        # If no positions exist, we need to create initial positions
        if not self.positions:
            return True
        
        # Calculate inventory status including position values (in USD)
        total_value_0 = self.balance_0 * current_price  # BTC to USD
        total_value_1 = self.balance_1                  # USDC already in USD
        
        # Add position values to total
        for position in self.positions:
            pos_value_0, pos_value_1 = self.calculate_position_value(position, current_price)
            total_value_0 += pos_value_0
            total_value_1 += pos_value_1
        
        total_value = total_value_0 + total_value_1
        
        if total_value == 0:
            return False
        
        current_ratio = total_value_0 / total_value
        
        # Use the inventory model to get the dynamic target ratio
        try:
            # Convert balances to wei for model (assuming 18 decimals for both tokens)
            token0_balance_wei = int(self.balance_0 * 1e18)
            token1_balance_wei = int(self.balance_1 * 1e18)
            
            # Mock client for token decimals
            class MockClient:
                def get_token_decimals(self, address):
                    return 18
            
            mock_client = MockClient()
            
            # Get model calculation
            model_result = self.inventory_model.calculate_lp_ranges(
                token0_balance=token0_balance_wei,
                token1_balance=token1_balance_wei,
                spot_price=current_price,
                price_history=self.price_history,
                token_a_address=self.config.TOKEN_A_ADDRESS,
                token_b_address=self.config.TOKEN_B_ADDRESS,
                client=mock_client
            )
            
            target_ratio = model_result.get('target_ratio', self.config.TARGET_INVENTORY_RATIO)
            deviation = abs(current_ratio - target_ratio)
            
        except Exception as e:
            logger.warning(f"Error getting model target ratio: {e}, using config default")
            target_ratio = self.config.TARGET_INVENTORY_RATIO
            deviation = abs(current_ratio - target_ratio)
        
        # Reasonable rebalancing threshold for backtesting
        # Use 10% threshold to avoid excessive rebalancing
        rebalance_threshold = 0.10  # 10% deviation threshold
        
        # Also check for significant price movements
        if len(self.price_history) >= 2:
            prev_price = self.price_history[-2]['price']
            price_change = abs(current_price - prev_price) / prev_price
            
            # Rebalance if price moved more than 5% (reasonable for backtesting)
            if price_change > 0.05:  # 5% threshold
                logger.info(f"Price movement trigger: {price_change:.2%} change from {prev_price:.2f} to {current_price:.2f}")
                return True
        
        should_rebalance = deviation > rebalance_threshold
        if should_rebalance:
            logger.info(f"Inventory deviation trigger: {deviation:.2%} > {rebalance_threshold:.2%} (current: {current_ratio:.2%}, target: {target_ratio:.2%})")
        
        return should_rebalance
    
    def rebalance_positions(self, current_price: float, timestamp: datetime) -> Dict[str, Any]:
        """
        Simulate rebalancing positions
        
        Args:
            current_price: Current price
            timestamp: Current timestamp
            
        Returns:
            Rebalancing result
        """
        # Collect fees from existing positions
        total_fees_0 = 0.0
        total_fees_1 = 0.0
        
        for position in self.positions:
            total_fees_0 += position.fees_collected_0
            total_fees_1 += position.fees_collected_1
        
        # Track fees separately to prevent compounding
        # Don't add fees to balances - they should be tracked separately
        self.total_fees_collected_0 += total_fees_0
        self.total_fees_collected_1 += total_fees_1
        
        # Burn existing positions (return original capital, not current market value)
        for position in self.positions:
            # Return only the original token amounts that were put into the position
            # This prevents compounding by not returning unrealized gains
            self.balance_0 += position.token0_amount  # Return original token0 amount
            self.balance_1 += position.token1_amount  # Return original token1 amount
        
        # Clear positions
        positions_burned = len(self.positions)
        self.positions = []
        
        # Calculate new ranges using inventory model
        # Convert balances to wei for model calculation
        token0_balance_wei = int(self.balance_0 * 10**18)
        token1_balance_wei = int(self.balance_1 * 10**18)
        
        # Mock client for token decimals
        class MockClient:
            def get_token_decimals(self, address):
                return 18
        
        mock_client = MockClient()
        
        ranges = self.inventory_model.calculate_lp_ranges(
            token0_balance=token0_balance_wei,
            token1_balance=token1_balance_wei,
            spot_price=current_price,
            price_history=self.price_history,
            token_a_address="0x0000000000000000000000000000000000000000",  # Dummy address
            token_b_address="0x0000000000000000000000000000000000000001",  # Dummy address
            client=mock_client
        )
        
        # Create new positions using calculated ranges
        # Only use available balances, not total portfolio value
        if self.balance_0 > 0 or self.balance_1 > 0:
            # Use ranges from inventory model
            range_a_pct = ranges['range_a_percentage'] / 100.0
            range_b_pct = ranges['range_b_percentage'] / 100.0
            
            # Calculate position sizes based on available balances
            # Position A: Above current price with range_a_pct
            # Position B: Below current price with range_b_pct
            
            # Use available balances directly for position sizing
            # Position A: Use available token0 balance
            # Position B: Use available token1 balance
            
            # Position A (above current price)
            position_a_upper = current_price * (1 + range_a_pct / 2)
            position_a_lower = current_price * (1 - range_a_pct / 2)
            
            # Position B (below current price)  
            position_b_upper = current_price * (1 + range_b_pct / 2)
            position_b_lower = current_price * (1 - range_b_pct / 2)
            
            # Calculate total portfolio value to determine rebalancing needs
            total_value_usd = (self.balance_0 * current_price) + self.balance_1
            target_value_0 = total_value_usd * 0.5  # 50% target
            target_value_1 = total_value_usd * 0.5  # 50% target
            
            # Calculate how much of each token we need to achieve target ratio
            current_value_0 = self.balance_0 * current_price
            current_value_1 = self.balance_1
            
            # Determine rebalancing action
            if current_value_0 > target_value_0:
                # Too much token0, need to sell some
                excess_value_0 = current_value_0 - target_value_0
                excess_token0 = excess_value_0 / current_price
                
                # Create position to sell excess token0 (above current price)
                position_0 = BacktestPosition(
                    token_id=f"pos_sell_{timestamp.timestamp()}",
                    token0_amount=excess_token0,
                    token1_amount=0.0,
                    tick_lower=position_a_lower,
                    tick_upper=position_a_upper,
                    liquidity=excess_token0 * current_price,
                    created_at=timestamp
                )
                
                # Keep remaining token1 in balance (no position needed)
                self.positions = [position_0]
                self.balance_0 -= excess_token0
                
            elif current_value_1 > target_value_1:
                # Too much token1, need to buy some token0
                excess_value_1 = current_value_1 - target_value_1
                excess_token1 = excess_value_1
                
                # Create position to buy token0 (below current price)
                position_1 = BacktestPosition(
                    token_id=f"pos_buy_{timestamp.timestamp()}",
                    token0_amount=0.0,
                    token1_amount=excess_token1,
                    tick_lower=position_b_lower,
                    tick_upper=position_b_upper,
                    liquidity=excess_token1,
                    created_at=timestamp
                )
                
                # Keep remaining token0 in balance (no position needed)
                self.positions = [position_1]
                self.balance_1 -= excess_token1
                
            else:
                # Already balanced, no positions needed
                self.positions = []
            
            logger.info(f"Created positions with ranges: A={range_a_pct:.3f}, B={range_b_pct:.3f}")
        
        rebalance_result = {
            'timestamp': timestamp,
            'price': current_price,
            'positions_burned': positions_burned,
            'fees_collected_0': total_fees_0,
            'fees_collected_1': total_fees_1,
            'new_positions': len(self.positions),
            'ranges': ranges
        }
        
        self.rebalances.append(rebalance_result)
        logger.info(f"Rebalanced at {timestamp}: burned {positions_burned} positions, created {len(self.positions)} new positions")
        
        return rebalance_result
    
    def run_backtest(self, 
                    ohlc_file: str,
                    initial_balance_0: float,
                    initial_balance_1: float,
                    start_date: Optional[datetime] = None,
                    end_date: Optional[datetime] = None) -> BacktestResult:
        """
        Run backtest on historical data
        
        Args:
            ohlc_file: Path to OHLC CSV file
            initial_balance_0: Initial token A balance
            initial_balance_1: Initial token B balance
            start_date: Start date for backtest (optional)
            end_date: End date for backtest (optional)
            
        Returns:
            BacktestResult with performance metrics
        """
        logger.info("Starting backtest...")
        
        # Load OHLC data
        df = self.load_ohlc_data(ohlc_file)
        
        # Filter by date range if specified
        if start_date:
            df = df[df['timestamp'] >= start_date]
        if end_date:
            df = df[df['timestamp'] <= end_date]
        
        if len(df) == 0:
            raise ValueError("No data in specified date range")
        
        # Initialize balances
        self.balance_0 = initial_balance_0
        self.balance_1 = initial_balance_1
        self.positions = []
        self.price_history = []
        self.trades = []
        self.rebalances = []
        
        start_time = df['timestamp'].iloc[0]
        end_time = df['timestamp'].iloc[-1]
        
        logger.info(f"Backtest period: {start_time} to {end_time}")
        logger.info(f"Initial balances: {initial_balance_0:.2f} token0, {initial_balance_1:.2f} token1")
        
        # Detect trades from OHLC data
        all_trades = self.detect_trades_from_ohlc(df)
        
        # Group trades by timestamp (minute)
        trade_groups = {}
        for trade in all_trades:
            minute_key = trade.timestamp.replace(second=0, microsecond=0)
            if minute_key not in trade_groups:
                trade_groups[minute_key] = []
            trade_groups[minute_key].append(trade)
        
        # Process each minute
        prev_portfolio_value = None
        
        for i, row in df.iterrows():
            timestamp = row['timestamp']
            current_price = row['close']
            
            # Update price history
            self.price_history.append({
                'timestamp': timestamp.timestamp(),
                'price': current_price
            })
            
            # Keep only recent price history
            if len(self.price_history) > self.config.VOLATILITY_WINDOW_SIZE:
                self.price_history = self.price_history[-self.config.VOLATILITY_WINDOW_SIZE:]
            
            # Process trades for this minute
            minute_key = timestamp.replace(second=0, microsecond=0)
            minute_trades = trade_groups.get(minute_key, [])
            
            if minute_trades:
                # Simulate LP fee collection (fees are already added to positions in simulate_lp_fees)
                fees_0, fees_1 = self.simulate_lp_fees(minute_trades, current_price)
                
                self.trades.extend(minute_trades)
            
            # Check if rebalancing is needed (with cooldown period)
            if self.should_rebalance(current_price):
                # Add cooldown period to prevent excessive rebalancing
                if self.last_rebalance_time is None or (timestamp - self.last_rebalance_time).total_seconds() > 86400:  # 24 hour cooldown
                    self.rebalance_positions(current_price, timestamp)
                    self.last_rebalance_time = timestamp
            
            # Track portfolio value
            current_portfolio_value = self.calculate_portfolio_value(current_price)
            self.portfolio_values.append(current_portfolio_value)
        
        # Calculate final results
        final_price = df['close'].iloc[-1]
        final_balance_0 = self.balance_0
        final_balance_1 = self.balance_1
        
        # Add position values to final balances
        for position in self.positions:
            final_balance_0 += position.token0_amount
            final_balance_1 += position.token1_amount
        
        # Calculate performance metrics
        initial_value = initial_balance_0 + (initial_balance_1 * df['close'].iloc[0])
        final_value = final_balance_0 + (final_balance_1 * final_price)
        total_return = (final_value - initial_value) / initial_value
        
        # Calculate total fees collected (already included in final balances)
        total_fees = sum(pos.fees_collected_0 + (pos.fees_collected_1 * final_price) for pos in self.positions)
        
        # Calculate performance metrics
        sharpe_ratio = self.calculate_sharpe_ratio()
        max_drawdown = self.calculate_max_drawdown()
        
        result = BacktestResult(
            start_time=start_time,
            end_time=end_time,
            initial_balance_0=initial_balance_0,
            initial_balance_1=initial_balance_1,
            final_balance_0=final_balance_0,
            final_balance_1=final_balance_1,
            total_fees_collected=total_fees,
            total_rebalances=len(self.rebalances),
            total_trades=len(self.trades),
            max_drawdown=max_drawdown,
            sharpe_ratio=sharpe_ratio,
            win_rate=0.0,  # TODO: Calculate
            trades=self.trades,
            rebalances=self.rebalances
        )
        
        logger.info(f"Backtest completed:")
        logger.info(f"  Total return: {total_return:.2%}")
        logger.info(f"  Total fees collected: {total_fees:.2f}")
        logger.info(f"  Total rebalances: {len(self.rebalances)}")
        logger.info(f"  Total trades: {len(self.trades)}")
        logger.info(f"  Sharpe ratio: {sharpe_ratio:.2f}")
        logger.info(f"  Max drawdown: {max_drawdown:.2%}")
        
        return result
    
    def save_results(self, result: BacktestResult, output_file: str):
        """
        Save backtest results to JSON file
        
        Args:
            result: BacktestResult object
            output_file: Output file path
        """
        # Convert to serializable format
        data = {
            'start_time': result.start_time.isoformat(),
            'end_time': result.end_time.isoformat(),
            'initial_balance_0': result.initial_balance_0,
            'initial_balance_1': result.initial_balance_1,
            'final_balance_0': result.final_balance_0,
            'final_balance_1': result.final_balance_1,
            'total_fees_collected': result.total_fees_collected,
            'total_rebalances': result.total_rebalances,
            'total_trades': result.total_trades,
            'trades': [
                {
                    'timestamp': trade.timestamp.isoformat(),
                    'price': trade.price,
                    'volume': trade.volume,
                    'trade_type': trade.trade_type,
                    'fees_paid': trade.fees_paid
                }
                for trade in result.trades
            ],
            'rebalances': result.rebalances
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"Results saved to {output_file}")

def main():
    """Example backtest usage"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Backtest Uniswap V3 LP Rebalancing Strategy')
    parser.add_argument('--ohlc-file', required=True, help='Path to OHLC CSV file')
    parser.add_argument('--initial-balance-0', type=float, default=1000.0, help='Initial token A balance')
    parser.add_argument('--initial-balance-1', type=float, default=1000.0, help='Initial token B balance')
    parser.add_argument('--start-date', help='Start date (YYYY-MM-DD)')
    parser.add_argument('--end-date', help='End date (YYYY-MM-DD)')
    parser.add_argument('--output', default='backtest_results.json', help='Output file for results')
    
    args = parser.parse_args()
    
    # Initialize configuration
    config = Config()
    
    # Initialize backtest engine
    engine = BacktestEngine(config)
    
    # Parse dates
    start_date = None
    end_date = None
    
    if args.start_date:
        start_date = datetime.strptime(args.start_date, '%Y-%m-%d')
    if args.end_date:
        end_date = datetime.strptime(args.end_date, '%Y-%m-%d')
    
    # Run backtest
    result = engine.run_backtest(
        ohlc_file=args.ohlc_file,
        initial_balance_0=args.initial_balance_0,
        initial_balance_1=args.initial_balance_1,
        start_date=start_date,
        end_date=end_date
    )
    
    # Save results
    engine.save_results(result, args.output)
    
    print(f"\nBacktest Results:")
    print(f"Period: {result.start_time} to {result.end_time}")
    print(f"Initial Value: {result.initial_balance_0 + result.initial_balance_1:.2f}")
    print(f"Final Value: {result.final_balance_0 + result.final_balance_1:.2f}")
    print(f"Total Return: {((result.final_balance_0 + result.final_balance_1) - (result.initial_balance_0 + result.initial_balance_1)) / (result.initial_balance_0 + result.initial_balance_1):.2%}")
    print(f"Total Fees Collected: {result.total_fees_collected:.2f}")
    print(f"Total Rebalances: {result.total_rebalances}")
    print(f"Total Trades: {result.total_trades}")

if __name__ == "__main__":
    main()
