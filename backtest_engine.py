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
from amm import AMMSimulator, SwapEvent
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
    """Represents a simulated trade - now uses SwapEvent from AMM"""
    timestamp: datetime
    price: float
    volume: float
    trade_type: str  # 'buy' or 'sell'
    fees_paid: float
    
    @classmethod
    def from_swap_event(cls, swap_event: SwapEvent) -> 'BacktestTrade':
        """Create BacktestTrade from SwapEvent"""
        return cls(
            timestamp=swap_event.timestamp,
            price=swap_event.price,
            volume=swap_event.volume_usd,  # Use volume_usd from swap event
            trade_type=swap_event.trade_type,
            fees_paid=swap_event.fees_token0 + swap_event.fees_token1  # Total fees
        )

@dataclass
class BacktestResult:
    """Results from a backtest run"""
    start_time: datetime
    end_time: datetime
    initial_balance_0: float
    initial_balance_1: float
    final_balance_0: float
    final_balance_1: float
    total_rebalances: int
    total_trades: int
    trades: List[BacktestTrade]
    rebalances: List[Dict[str, Any]]
    # Performance metrics
    token0_return: float = 0.0
    token1_return: float = 0.0
    final_inventory_deviation: float = 0.0
    token0_drawdown: float = 0.0
    token1_drawdown: float = 0.0

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
            falloff_factor = max(self.config.POSITION_FALLOFF_FACTOR, 1.0 - distance_from_center * 0.5)
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
            
        
        total_value = balance_value_0 + balance_value_1 + position_value_0 + position_value_1
        return total_value
    
    
    def calculate_token_drawdown(self, token_index: int) -> float:
        """
        Calculate maximum drawdown for a specific token
        
        Args:
            token_index: 0 for token0, 1 for token1
            
        Returns:
            Maximum drawdown as percentage
        """
        if len(self.portfolio_values) < 2:
            return 0.0
        
        # For now, use portfolio value drawdown as proxy for token drawdown
        # This is a simplification - in reality we'd need to track token balances over time
        values = np.array(self.portfolio_values)
        peak = np.maximum.accumulate(values)
        drawdown = (peak - values) / peak
        max_drawdown = np.max(drawdown)
        
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
        
        # Use the initial target ratio instead of model's target ratio
        target_ratio = self.initial_target_ratio
        deviation = abs(current_ratio - target_ratio)
        
        # Rebalance only on inventory deviation
        # Use configurable threshold to avoid excessive rebalancing
        rebalance_threshold = self.config.REBALANCE_THRESHOLD
        
        should_rebalance = deviation > rebalance_threshold
        if should_rebalance:
            logger.info(f"Inventory deviation trigger: {deviation:.2%} > {rebalance_threshold:.2%} (current: {current_ratio:.2%}, target: {target_ratio:.2%})")
        
        return should_rebalance
    
    def rebalance_positions(self, current_price: float, timestamp: datetime, amm_simulator: AMMSimulator) -> Dict[str, Any]:
        """
        Simulate rebalancing positions
        
        Args:
            current_price: Current price
            timestamp: Current timestamp
            
        Returns:
            Rebalancing result
        """
        # Check if AMM has active positions, if so use those balances for rebalancing
        if amm_simulator.has_active_positions():
            # Get balances from active AMM positions
            amm_token0_balance, amm_token1_balance = amm_simulator.get_active_positions_balances()
            logger.info(f"Using AMM active position balances: token0={amm_token0_balance:.6f}, token1={amm_token1_balance:.6f}")
            
            # Use AMM balances for rebalancing calculation
            rebalance_token0_balance = amm_token0_balance
            rebalance_token1_balance = amm_token1_balance
        else:
            # No active positions, use initial balances
            logger.info(f"No active AMM positions, using initial balances: token0={self.balance_0:.6f}, token1={self.balance_1:.6f}")
            rebalance_token0_balance = self.balance_0
            rebalance_token1_balance = self.balance_1
        
        # Burn old positions - clear both the backtest positions and AMM pool positions
        positions_burned = len(self.positions)
        self.positions = []
        
        # Clear AMM pool positions to prepare for new ones
        amm_simulator.clear_all_positions()
        
        # Calculate new ranges using inventory model with the appropriate balances
        # Convert balances to wei for model calculation
        token0_balance_wei = int(rebalance_token0_balance * 10**18)
        token1_balance_wei = int(rebalance_token1_balance * 10**18)
        
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
        # Only use available balances from AMM positions
        if rebalance_token0_balance > 0 or rebalance_token1_balance > 0:
            # Use ranges from inventory model
            range_a_pct = ranges['range_a_percentage'] / 100.0
            range_b_pct = ranges['range_b_percentage'] / 100.0
            
            # Calculate position sizes based on available balances
            # Position A: Token0 range - ABOVE current price (for selling token0 as price rises)
            # Position B: Token1 range - BELOW current price (for buying token0 as price falls)
            
            # Position A (token0 range - ABOVE current price)
            # This position is active when price is above current_price
            position_a_lower = current_price  # Start at current price
            position_a_upper = current_price * (1 + range_a_pct)  # Extend upward
            
            # Position B (token1 range - BELOW current price)  
            # This position is active when price is below current_price
            position_b_lower = current_price * (1 - range_b_pct)  # Extend downward
            position_b_upper = current_price  # End at current price
            
            # Calculate total portfolio value to determine rebalancing needs
            total_value_usd = (rebalance_token0_balance * current_price) + rebalance_token1_balance
            target_value_0 = total_value_usd * 0.5  # 50% target
            target_value_1 = total_value_usd * 0.5  # 50% target
            
            # Calculate how much of each token we need to achieve target ratio
            current_value_0 = rebalance_token0_balance * current_price
            current_value_1 = rebalance_token1_balance
            
            # Rebalance the portfolio to achieve initial target ratio
            # Calculate how much of each token we need for initial ratio
            target_token0_value = total_value_usd * self.initial_target_ratio
            target_token1_value = total_value_usd * (1 - self.initial_target_ratio)
            
            # Calculate current token values
            current_token0_value = rebalance_token0_balance * current_price
            current_token1_value = rebalance_token1_balance
            
            # Calculate rebalancing amounts
            token0_excess = current_token0_value - target_token0_value
            token1_excess = current_token1_value - target_token1_value
            
            # Rebalance by adjusting token amounts
            if token0_excess > 0:
                # Too much token0, convert excess to token1
                token0_to_sell = token0_excess / current_price
                token1_to_buy = token0_excess
                rebalance_token0_balance -= token0_to_sell
                rebalance_token1_balance += token1_to_buy
            elif token1_excess > 0:
                # Too much token1, convert excess to token0
                token1_to_sell = token1_excess
                token0_to_buy = token1_excess / current_price
                rebalance_token0_balance += token0_to_buy
                rebalance_token1_balance -= token1_to_sell
            
            # Now create positions with the rebalanced amounts using AMM mint_position
            # Position A: Above current price (sell token0 for token1) - use ALL token0
            position_a_token0 = rebalance_token0_balance  # Use ALL rebalanced token0
            position_a_token1 = 0.0
            
            # Use AMM Simulator to mint position with proper Uniswap V3 liquidity calculation
            liquidity_a, adjusted_token0_a, adjusted_token1_a = amm_simulator.mint_position(
                token0_amount=position_a_token0,
                token1_amount=position_a_token1,
                tick_lower=position_a_lower,
                tick_upper=position_a_upper,
                current_price=current_price
            )
            
            position_0 = BacktestPosition(
                token_id=f"pos_sell_{timestamp.timestamp()}",
                token0_amount=adjusted_token0_a,
                token1_amount=adjusted_token1_a,
                tick_lower=position_a_lower,
                tick_upper=position_a_upper,
                liquidity=liquidity_a,
                created_at=timestamp
            )
            
            # Position B: Below current price (buy token0 with token1) - use ALL token1
            position_b_token0 = 0.0
            position_b_token1 = rebalance_token1_balance  # Use ALL rebalanced token1
            
            # Use AMM Simulator to mint position with proper Uniswap V3 liquidity calculation
            liquidity_b, adjusted_token0_b, adjusted_token1_b = amm_simulator.mint_position(
                token0_amount=position_b_token0,
                token1_amount=position_b_token1,
                tick_lower=position_b_lower,
                tick_upper=position_b_upper,
                current_price=current_price
            )
            
            position_1 = BacktestPosition(
                token_id=f"pos_buy_{timestamp.timestamp()}",
                token0_amount=adjusted_token0_b,
                token1_amount=adjusted_token1_b,
                tick_lower=position_b_lower,
                tick_upper=position_b_upper,
                liquidity=liquidity_b,
                created_at=timestamp
            )
            
            # Deploy positions and update balances
            self.positions = [position_0, position_1]
            # Don't update engine balances - AMM simulator already tracks them
            # self.balance_0 -= adjusted_token0_a + adjusted_token0_b
            # self.balance_1 -= adjusted_token1_a + adjusted_token1_b
            
            # Don't manually update AMM balances - they're already correct
            # amm_simulator.current_token0_balance = rebalance_token0_balance - adjusted_token0_a - adjusted_token0_b
            # amm_simulator.current_token1_balance = rebalance_token1_balance - adjusted_token1_a - adjusted_token1_b
            
            logger.info(f"Created positions with ranges: A={range_a_pct:.3f}, B={range_b_pct:.3f}")
        
        rebalance_result = {
            'timestamp': timestamp,
            'price': current_price,
            'positions_burned': positions_burned,
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
        
        # Calculate initial target ratio based on initial balances
        initial_price = df['close'].iloc[0]
        initial_value_0 = initial_balance_0 * initial_price
        initial_value_1 = initial_balance_1
        initial_total_value = initial_value_0 + initial_value_1
        self.initial_target_ratio = initial_value_0 / initial_total_value if initial_total_value > 0 else 0.5
        
        start_time = df['timestamp'].iloc[0]
        end_time = df['timestamp'].iloc[-1]
        
        logger.info(f"Backtest period: {start_time} to {end_time}")
        logger.info(f"Initial balances: {initial_balance_0:.2f} token0, {initial_balance_1:.2f} token1")
        
        # Initialize AMM Simulator for event-driven trade detection
        amm_simulator = AMMSimulator(
            fee_tier_bps=self.fee_tier_bps,
            trade_detection_threshold=self.config.TRADE_DETECTION_THRESHOLD
        )
        
        # Set initial balances in AMM Simulator
        amm_simulator.set_initial_balances(initial_balance_0, initial_balance_1)
        
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
            
            # Process OHLC row through AMM Simulator
            swap_event = amm_simulator.compute(row)
            
            if swap_event:
                # Convert swap event to trade
                trade = BacktestTrade.from_swap_event(swap_event)
                self.trades.append(trade)
                
                # Update our balances with new balances from swap event
                self.balance_0 = swap_event.new_token0_balance
                self.balance_1 = swap_event.new_token1_balance
            
            # Check if rebalancing is needed
            if self.should_rebalance(current_price):
                self.rebalance_positions(current_price, timestamp, amm_simulator)
                self.last_rebalance_time = timestamp
            
            # Track portfolio value
            current_portfolio_value = self.calculate_portfolio_value(current_price)
            self.portfolio_values.append(current_portfolio_value)
        
        # Calculate final results
        initial_price = df['close'].iloc[0]
        final_price = df['close'].iloc[-1]
        
        # Get final balances from AMM (includes all position values)
        if amm_simulator.has_active_positions():
            final_balance_0, final_balance_1 = amm_simulator.get_active_positions_balances()
        else:
            final_balance_0 = self.balance_0
            final_balance_1 = self.balance_1
        
        # Calculate performance metrics
        initial_value = initial_balance_0 + (initial_balance_1 * df['close'].iloc[0])
        final_value = final_balance_0 + (final_balance_1 * final_price)
        total_return = (final_value - initial_value) / initial_value
        
        # Calculate performance metrics
        # Token0 return: (final - initial) / initial
        # Note: Final balances already include all fees earned through trading
        token0_return = (final_balance_0 - initial_balance_0) / initial_balance_0 if initial_balance_0 > 0 else 0.0
        
        # Token1 return: (final - initial) / initial  
        token1_return = (final_balance_1 - initial_balance_1) / initial_balance_1 if initial_balance_1 > 0 else 0.0
        
        # Calculate final inventory deviation
        final_value_0 = final_balance_0 * final_price
        final_value_1 = final_balance_1
        final_total_value = final_value_0 + final_value_1
        final_current_ratio = final_value_0 / final_total_value if final_total_value > 0 else 0.0
        final_inventory_deviation = abs(final_current_ratio - self.initial_target_ratio)
        
        # Calculate drawdowns for each token
        token0_drawdown = self.calculate_token_drawdown(0)  # Token0 drawdown
        token1_drawdown = self.calculate_token_drawdown(1)  # Token1 drawdown
        
        result = BacktestResult(
            start_time=start_time,
            end_time=end_time,
            initial_balance_0=initial_balance_0,
            initial_balance_1=initial_balance_1,
            final_balance_0=final_balance_0,
            final_balance_1=final_balance_1,
            total_rebalances=len(self.rebalances),
            total_trades=len(self.trades),
            trades=self.trades,
            rebalances=self.rebalances
        )
        
        # Add new metrics to result
        result.token0_return = token0_return
        result.token1_return = token1_return
        result.token0_drawdown = token0_drawdown
        result.token1_drawdown = token1_drawdown
        result.final_inventory_deviation = final_inventory_deviation
        
        logger.info(f"Backtest completed:")
        logger.info(f"  Total return: {total_return:.2%}")
        logger.info(f"  Token0 return: {token0_return:.2%}")
        logger.info(f"  Token1 return: {token1_return:.2%}")
        logger.info(f"  Total rebalances: {len(self.rebalances)}")
        logger.info(f"  Total trades: {len(self.trades)}")
        logger.info(f"  Token0 drawdown: {token0_drawdown:.2%}")
        logger.info(f"  Token1 drawdown: {token1_drawdown:.2%}")
        
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
            'rebalances': [
                {
                    'timestamp': rebalance['timestamp'].isoformat() if hasattr(rebalance['timestamp'], 'isoformat') else str(rebalance['timestamp']),
                    'price': rebalance['price'],
                    'positions_burned': rebalance['positions_burned'],
                    'new_positions': rebalance['new_positions'],
                    'ranges': rebalance['ranges']
                }
                for rebalance in result.rebalances
            ]
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"Results saved to {output_file}")


