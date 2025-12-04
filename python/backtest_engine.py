#!/usr/bin/env python3
"""
AsymmetricLP - Backtesting Module
Simulates asymmetric LP rebalancing using historical OHLC data
"""
import logging
import math
import pandas as pd
import numpy as np
from typing import Dict, List, Tuple, Any, Optional
from datetime import datetime, timedelta
import json
from dataclasses import dataclass
from config import Config
from models.model_factory import ModelFactory
from strategy import AsymmetricLPStrategy
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
    fees_earned: float  # Fees earned by LP (us), not paid by LP
    new_token0_balance: float = 0.0
    new_token1_balance: float = 0.0
    range_a_percentage: float = 0.0  # upper band width (%) active since last rebalance
    range_b_percentage: float = 0.0  # lower band width (%) active since last rebalance
    
    @classmethod
    def from_swap_event(cls, swap_event: SwapEvent, range_a_pct_percent: float = 0.0, range_b_pct_percent: float = 0.0) -> 'BacktestTrade':
        """Create BacktestTrade from SwapEvent"""
        return cls(
            timestamp=swap_event.timestamp,
            price=swap_event.price,
            volume=swap_event.volume_usd,  # Use volume_usd from swap event
            trade_type=swap_event.trade_type,
            fees_earned=swap_event.fees_token0 + swap_event.fees_token1,  # Fees earned by LP
            new_token0_balance=swap_event.new_token0_balance,
            new_token1_balance=swap_event.new_token1_balance,
            range_a_percentage=range_a_pct_percent,
            range_b_percentage=range_b_pct_percent,
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
    initial_target_ratio: float = 0.5

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
        self.strategy = AsymmetricLPStrategy(config, self.inventory_model)
        
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
        # Baselines set at each rebalance (for edge-triggered rebalances)
        self.last_rebalance_token0: float = 0.0
        self.last_rebalance_token1: float = 0.0
        self.last_rebalance_price: Optional[float] = None
        
        # Performance tracking
        self.portfolio_values = []  # Track portfolio value over time
        
        # Fee tier in basis points (e.g., 3000 = 0.3%)
        self.fee_tier_bps = config.FEE_TIER
        
        logger.info("Backtest engine initialized")
        # Track whether we've created the very first positions
        self.initial_positions_created: bool = False
        # Track initial token inventories (in token units)
        self.initial_token0: float = 0.0
        self.initial_token1: float = 0.0
    
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
        # token0 is USDC (already USD); token1 is ETH (convert via 1/price)
        value_0 = position.token0_amount
        value_1 = position.token1_amount * (1.0 / current_price)
        
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
        # token0 is USDC (already USD); token1 is ETH (convert via 1/price)
        balance_value_0 = self.balance_0
        balance_value_1 = self.balance_1 * (1.0 / current_price)
        
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
        Determine if rebalancing is needed based on per-token inventory depletion.
        Uses the shared strategy logic for consistency between backtest and live.
        
        Args:
            current_price: Current price
            
        Returns:
            True if rebalancing is needed
        """
        return self.strategy.should_rebalance(
            current_price=current_price,
            price_history=self.price_history,
            current_token0=self.balance_0,
            current_token1=self.balance_1,
            last_rebalance_token0=self.last_rebalance_token0 if self.last_rebalance_token0 > 0.0 else None,
            last_rebalance_token1=self.last_rebalance_token1 if self.last_rebalance_token1 > 0.0 else None,
            last_rebalance_price=self.last_rebalance_price,
            has_positions=len(self.positions) > 0,
        )
    
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
        
        # Compute ranges and adjusted balances via shared strategy API
        class MockClient:
            def get_token_decimals(self, address):
                return 18
        mock_client = MockClient()
        startup_allocation = not self.initial_positions_created and not self.positions
        range_a_pct, range_b_pct, rebalance_token0_balance, rebalance_token1_balance, ranges = (
            self.strategy.plan_rebalance(
                current_price=current_price,
                price_history=self.price_history,
                token0_balance=rebalance_token0_balance,
                token1_balance=rebalance_token1_balance,
                initial_target_ratio=self.initial_target_ratio,
                startup_allocation=startup_allocation,
                client=mock_client,
                initial_token0_units=self.initial_token0,
                initial_token1_units=self.initial_token1,
                do_conversion=startup_allocation,
            )
        )

        # On first mint with untouched starting balances, show zero deviation to the model/outputs
        if startup_allocation and abs(rebalance_token0_balance - self.initial_token0) < 1e-9 and abs(rebalance_token1_balance - self.initial_token1) < 1e-12:
            if isinstance(ranges, dict):
                ranges = ranges.copy()
                # Align reported inventory ratio with target and set deviation to 0
                if 'target_ratio' in ranges:
                    ranges['inventory_ratio'] = ranges.get('target_ratio')
                ranges['deviation'] = 0.0
        
        # Create new positions using calculated ranges
        if rebalance_token0_balance > 0 or rebalance_token1_balance > 0:
            # Tick-align model percentages already computed (fractions)
            
            # Raw percentage bands around current price
            upper_lower_raw = current_price
            upper_upper_raw = current_price * (1 + range_a_pct)
            lower_lower_raw = current_price * (1 - range_b_pct)
            lower_upper_raw = current_price

            # Helpers
            def price_to_tick(p: float) -> int:
                return math.floor(math.log(p) / math.log(1.0001))

            def tick_to_price(t: int) -> float:
                return math.pow(1.0001, t)

            # Delegate band computation + mint to AMM (single math source of truth)
            total_L, L0, L1, (position_a_lower, position_a_upper), (position_b_lower, position_b_upper) = (
                amm_simulator.mint_bands_percent(
                    current_price=current_price,
                    range_a_pct=range_a_pct,
                    range_b_pct=range_b_pct,
                    token0_amount=rebalance_token0_balance,
                    token1_amount=rebalance_token1_balance,
                )
            )
            
            # Reflect minted positions for backtester bookkeeping
            adjusted_token0_a = rebalance_token0_balance if rebalance_token0_balance > 0 else 0.0
            adjusted_token1_a = 0.0
            position_0 = BacktestPosition(
                token_id=f"pos_sell_{timestamp.timestamp()}",
                token0_amount=adjusted_token0_a,
                token1_amount=adjusted_token1_a,
                tick_lower=position_a_lower,
                tick_upper=position_a_upper,
                liquidity=L0,
                created_at=timestamp
            )
            
            adjusted_token0_b = 0.0
            adjusted_token1_b = rebalance_token1_balance if rebalance_token1_balance > 0 else 0.0
            position_1 = BacktestPosition(
                token_id=f"pos_buy_{timestamp.timestamp()}",
                token0_amount=adjusted_token0_b,
                token1_amount=adjusted_token1_b,
                tick_lower=position_b_lower,
                tick_upper=position_b_upper,
                liquidity=L1,
                created_at=timestamp
            )
            
            # Deploy positions and update balances
            self.positions = [position_0, position_1]
            if startup_allocation:
                self.initial_positions_created = True

            # Update engine-side balances to reflect AMM state post-mint
            self.balance_0, self.balance_1 = amm_simulator.get_active_positions_balances()
            # Capture state for JSON: first mint, next rebalance, and first trade after
            if not hasattr(self, '_debug_capture'):
                self._debug_capture = {
                    'first_mint': None,
                    'second_rebalance': None,
                    'first_trade_after_second_rebalance': None
                }
            # Record first mint snapshot
            if self._debug_capture['first_mint'] is None:
                self._debug_capture['first_mint'] = {
                    'timestamp': timestamp.isoformat(),
                    'price': current_price,
                    'post_mint_balances': {'token0': self.balance_0, 'token1': self.balance_1},
                    'ranges': {'A': [position_a_lower, position_a_upper], 'B': [position_b_lower, position_b_upper]},
                }
            else:
                # If first mint exists and second rebalance not yet captured, this is the second rebalance
                if self._debug_capture['second_rebalance'] is None:
                    self._debug_capture['second_rebalance'] = {
                        'timestamp': timestamp.isoformat(),
                        'price': current_price,
                        'post_rebalance_balances': {'token0': self.balance_0, 'token1': self.balance_1},
                        'ranges': {'A': [position_a_lower, position_a_upper], 'B': [position_b_lower, position_b_upper]},
                    }
            # Update rebalance baselines (edge-triggered logic)
            self.last_rebalance_token0 = self.balance_0
            self.last_rebalance_token1 = self.balance_1
            self.last_rebalance_price = current_price
            
            logger.info(f"Created tick-aligned positions: A=[{position_a_lower:.8f},{position_a_upper:.8f}] B=[{position_b_lower:.8f},{position_b_upper:.8f}] (ranges A={ranges['range_a_percentage']}%, B={ranges['range_b_percentage']}%)")
            # Save current active range percentages (in percent units) for later inclusion on trades
            self._active_range_a_pct_percent = float(ranges.get('range_a_percentage', 0.0))
            self._active_range_b_pct_percent = float(ranges.get('range_b_percentage', 0.0))
        
        rebalance_result = {
            'timestamp': timestamp,
            'price': current_price,
            'positions_burned': positions_burned,
            'new_positions': len(self.positions),
            'ranges': {k: (None if k == 'target_ratio' else v) for k, v in ranges.items()},
            'target_units': {'token0': self.initial_token0, 'token1': self.initial_token1},
            'is_initial_mint': startup_allocation
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
        self.initial_token0 = initial_balance_0
        self.initial_token1 = initial_balance_1
        self.positions = []
        self.price_history = []
        self.trades = []
        self.rebalances = []
        
        # Calculate initial target ratio based on initial balances (USD units)
        # token0 is already in token0 units (USD); token1 valued via 1/price
        initial_price = df['close'].iloc[0]
        initial_value_0 = initial_balance_0
        initial_value_1 = initial_balance_1 * (1 / initial_price)
        initial_total_value = initial_value_0 + initial_value_1
        self.initial_target_ratio = initial_value_0 / initial_total_value if initial_total_value > 0 else 0.5
        # Align model target to starting balances (not hardcoded 50/50)
        try:
            setattr(self.config, 'TARGET_INVENTORY_RATIO', self.initial_target_ratio)
        except Exception:
            pass
        # Also propagate to the instantiated model so its cached value updates
        if hasattr(self.inventory_model, 'target_inventory_ratio'):
            self.inventory_model.target_inventory_ratio = float(self.initial_target_ratio)
        
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
                trade = BacktestTrade.from_swap_event(
                    swap_event,
                    getattr(self, '_active_range_a_pct_percent', 0.0),
                    getattr(self, '_active_range_b_pct_percent', 0.0)
                )
                self.trades.append(trade)
                
                # Update our balances with new balances from swap event
                self.balance_0 = swap_event.new_token0_balance
                self.balance_1 = swap_event.new_token1_balance
                # If we've already captured the second rebalance, capture the very first trade after it
                if hasattr(self, '_debug_capture') and self._debug_capture.get('second_rebalance') and not self._debug_capture.get('first_trade_after_second_rebalance'):
                    self._debug_capture['first_trade_after_second_rebalance'] = {
                        'timestamp': trade.timestamp.isoformat(),
                        'price': trade.price,
                        'trade_type': trade.trade_type,
                        'fees_earned': trade.fees_earned,
                        'post_trade_balances': {'token0': self.balance_0, 'token1': self.balance_1}
                    }
            
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
        
        # Calculate performance metrics in USD (token0 is USD; token1 valued via 1/price)
        initial_value = initial_balance_0 + (initial_balance_1 * (1.0 / initial_price))
        final_value = final_balance_0 + (final_balance_1 * (1.0 / final_price))
        total_return = (final_value - initial_value) / initial_value
        
        # Calculate performance metrics
        # Token0 return: (final - initial) / initial
        # Note: Final balances already include all fees earned through trading
        token0_return = (final_balance_0 - initial_balance_0) / initial_balance_0 if initial_balance_0 > 0 else 0.0
        
        # Token1 return: (final - initial) / initial  
        token1_return = (final_balance_1 - initial_balance_1) / initial_balance_1 if initial_balance_1 > 0 else 0.0
        
        # Calculate final inventory deviation
        final_value_0 = final_balance_0
        final_value_1 = final_balance_1 * (1.0 / final_price)
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
            rebalances=self.rebalances,
            initial_target_ratio=self.initial_target_ratio
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
        # Build interleaved timeline of rebalances and trades
        # Use original datetime objects for sorting, then serialize
        timeline = []
        for t in result.trades:
            timeline.append({
                '_dt': t.timestamp,
                'type': 'trade',
                'timestamp': t.timestamp.isoformat(),
                'price': t.price,
                'volume': t.volume,
                'trade_type': t.trade_type,
                'fees_earned': t.fees_earned,
                'new_token0_balance': t.new_token0_balance,
                'new_token1_balance': t.new_token1_balance,
                'range_a_percentage': t.range_a_percentage,
                'range_b_percentage': t.range_b_percentage
            })
        for r in result.rebalances:
            ts = r['timestamp'] if not hasattr(r['timestamp'], 'isoformat') else r['timestamp']
            # Normalize to datetime
            try:
                dt = ts if hasattr(ts, 'isoformat') else datetime.fromisoformat(str(ts))
            except Exception:
                dt = None
            timeline.append({
                '_dt': dt,
                'type': 'rebalance',
                'timestamp': (ts.isoformat() if hasattr(ts, 'isoformat') else str(ts)),
                'price': r.get('price'),
                'positions_burned': r.get('positions_burned'),
                'new_positions': r.get('new_positions'),
                'ranges': r.get('ranges', {}),
                'is_initial_mint': r.get('is_initial_mint', False)
            })
        # Sort by datetime and drop helper
        timeline.sort(key=lambda e: (e['_dt'] or datetime.min))
        for e in timeline:
            e.pop('_dt', None)

        data = {
            'start_time': result.start_time.isoformat(),
            'end_time': result.end_time.isoformat(),
            'initial_balance_0': result.initial_balance_0,
            'initial_balance_1': result.initial_balance_1,
            'final_balance_0': result.final_balance_0,
            'final_balance_1': result.final_balance_1,
            'total_rebalances': result.total_rebalances,
            'total_trades': result.total_trades,
            'events': timeline,
            'trades': [
                {
                    'timestamp': trade.timestamp.isoformat(),
                    'price': trade.price,
                    'volume': trade.volume,
                    'trade_type': trade.trade_type,
                    'fees_earned': trade.fees_earned,
                    'new_token0_balance': trade.new_token0_balance,
                    'new_token1_balance': trade.new_token1_balance,
                    'range_a_percentage': trade.range_a_percentage,
                    'range_b_percentage': trade.range_b_percentage
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
            ],
            'debug': getattr(self, '_debug_capture', None)
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"Results saved to {output_file}")


