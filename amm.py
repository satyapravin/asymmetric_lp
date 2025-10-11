"""
AMM Simulator for simulating Uniswap V3 liquidity changes and swap events
Based on Uniswap V3 math from Skewed_LP_AMM notebook
"""
import pandas as pd
import math
from typing import List, Tuple, Optional
from dataclasses import dataclass
from datetime import datetime
from abc import ABC, abstractmethod
import logging

logger = logging.getLogger(__name__)

@dataclass
class SwapEvent:
    """Represents a swap event with updated token balances"""
    timestamp: datetime
    price: float
    trade_type: str  # 'buy' or 'sell'
    liquidity_size: float  # Size of the liquidity change
    liquidity_share: float  # Share of total liquidity this change represents
    fees_token0: float  # Fees collected in token0
    fees_token1: float  # Fees collected in token1
    volume_usd: float  # Trade volume in USD
    new_token0_balance: float  # Token0 balance after swap
    new_token1_balance: float  # Token1 balance after swap


class Record:
    """Record for tracking range position state changes"""
    def __init__(self):
        self.balance_token1 = 0.0
        self.balance_token0 = 0.0
        self.sold_token0 = 0.0
        self.bought_token0 = 0.0
        self.sold_token1 = 0.0
        self.bought_token1 = 0.0
        self.fees_token0 = 0.0
        self.fees_token1 = 0.0
        self.range_lower = 0.0
        self.range_upper = 0.0


class UniswapV3SingleSidedRange(ABC):
    """
    Base class for single-sided Uniswap V3 range positions
    Implements proper Uniswap V3 math for liquidity and swaps
    """
    def __init__(self, fee_tier):
        self.fee_tier = fee_tier
        self.balance_token1 = 0.0
        self.balance_token0 = 0.0
        self.sold_token0 = 0.0
        self.bought_token0 = 0.0
        self.sold_token1 = 0.0
        self.bought_token1 = 0.0
        self.fees_token0 = 0.0
        self.fees_token1 = 0.0
        self.liquidity = 0.0
        self.range_lower = 0.0
        self.range_upper = 0.0
        self.last_spot_price = 0.0

    def settle(self, record):
        """Settle position state into a record"""
        # First update internal balances based on swaps
        self.balance_token0 += -self.sold_token0 + self.bought_token0
        self.balance_token1 += -self.sold_token1 + self.bought_token1
        
        # Then copy the UPDATED balances to the record
        record.balance_token1 = self.balance_token1
        record.balance_token0 = self.balance_token0
        record.sold_token0 = self.sold_token0
        record.bought_token0 = self.bought_token0
        record.sold_token1 = self.sold_token1
        record.bought_token1 = self.bought_token1
        record.fees_token0 = self.fees_token0
        record.fees_token1 = self.fees_token1
        record.range_lower = self.range_lower
        record.range_upper = self.range_upper

        # Reset swap counters for next iteration
        self.sold_token0 = 0.0
        self.bought_token0 = 0.0
        self.sold_token1 = 0.0
        self.bought_token1 = 0.0

    def compute(self):
        """Compute liquidity from token balances"""
        self.compute_token0_liquidity()
        self.compute_token1_liquidity()

    def compute_token0_liquidity(self):
        """Calculate liquidity from token0 balance"""
        if self.balance_token0 <= 0:
            return
        numerator = self.balance_token0 * math.sqrt(self.range_lower) * math.sqrt(self.range_upper)
        denominator = math.sqrt(self.range_upper) - math.sqrt(self.range_lower)
        self.liquidity = numerator / denominator

    def compute_token1_liquidity(self):
        """Calculate liquidity from token1 balance"""
        if self.balance_token1 <= 0:
            return
        sqrt_pc = math.sqrt(self.range_upper)
        sqrt_pa = math.sqrt(self.range_lower)
        delta_price = sqrt_pc - sqrt_pa
        self.liquidity = self.balance_token1 / delta_price

    def swap(self, price):
        """
        Process a swap at the given price
        Updates token balances based on Uniswap V3 math
        """
        if price > self.range_lower and price < self.range_upper:  # Within range
            if price >= self.last_spot_price:  # Price moving upwards - selling token0, buying token1
                if self.balance_token0 > 0:
                    ref_price = min(price, self.range_upper)
                    self.sold_token0 = self.token0_sold(ref_price, self.last_spot_price, self.liquidity)
                    self.bought_token1 = self.token1_bought(ref_price, self.last_spot_price, self.liquidity)
                    self.fees_token0 = self.sold_token0 * self.fee_tier
                else:
                    pass  # No token0 to sell
            else:  # Price moving downwards - buying token0, selling token1
                if self.balance_token1 > 0:
                    ref_price = max(price, self.range_lower)
                    self.bought_token0 += self.token0_bought(ref_price, self.last_spot_price, self.liquidity)
                    self.sold_token1 = self.token1_sold(ref_price, self.last_spot_price, self.liquidity)
                    self.fees_token1 = self.sold_token1 * self.fee_tier
                else:
                    pass  # No token1 to sell

            self.last_spot_price = price

    def token0_sold(self, current_price, lower, liquidity):
        """Calculate token0 sold when price moves up"""
        numerator = math.sqrt(current_price) - math.sqrt(lower)
        denominator = math.sqrt(current_price) * math.sqrt(lower)
        sold_token0 = numerator * liquidity / denominator
        return sold_token0

    def token0_bought(self, current_price, upper, liquidity):
        """Calculate token0 bought when price moves down"""
        numerator = math.sqrt(upper) - math.sqrt(current_price)
        denominator = math.sqrt(current_price) * math.sqrt(upper)
        bought_token0 = numerator * liquidity / denominator
        return bought_token0

    def token1_sold(self, current_price, upper, liquidity):
        """Calculate token1 sold when price moves down"""
        return (math.sqrt(upper) - math.sqrt(current_price)) * liquidity

    def token1_bought(self, current_price, lower, liquidity):
        """Calculate token1 bought when price moves up"""
        return (math.sqrt(current_price) - math.sqrt(lower)) * liquidity


class UniswapV3RangeToken1(UniswapV3SingleSidedRange):
    """
    Single-sided range position for token1 (quote/stablecoin)
    Position is active when price is below current price
    """
    def __init__(self, fee_tier, range_lower, range_upper, amount):
        super().__init__(fee_tier)
        self.balance_token1 = amount
        self.range_lower = range_lower
        self.range_upper = range_upper
        self.last_spot_price = range_upper
        self.compute()


class UniswapV3RangeToken0(UniswapV3SingleSidedRange):
    """
    Single-sided range position for token0 (base/volatile asset)
    Position is active when price is above current price
    """
    def __init__(self, fee_tier, range_lower, range_upper, amount):
        super().__init__(fee_tier)
        self.balance_token0 = amount
        self.range_lower = range_lower
        self.range_upper = range_upper
        self.last_spot_price = range_lower
        self.compute()


class UniswapV3Pool:
    """
    Uniswap V3 Pool simulator with single-sided range positions
    Supports asymmetric liquidity provision
    """
    def __init__(self, pool_fee_percent):
        # As per UniswapV3 settings, for USDT-token0 pool, currency0 = token0 (base) and currency1 = token1 (quote)
        self.base_fee_percent = pool_fee_percent
        self.token1_range = None  # Quote token range (below current price)
        self.token0_range = None  # Base token range (above current price)

    def clear_quote_token1(self):
        """Clear token1 (quote) range position"""
        self.token1_range = None

    def clear_quote_token0(self):
        """Clear token0 (base) range position"""
        self.token0_range = None

    def quote_token1(self, amount, current_price, price_lower):
        """
        Create a token1 (quote) range position
        Active when price is between price_lower and current_price
        """
        self.token1_range = UniswapV3RangeToken1(self.base_fee_percent, price_lower, current_price, amount)

    def quote_token0(self, amount, current_price, price_upper):
        """
        Create a token0 (base) range position
        Active when price is between current_price and price_upper
        """
        self.token0_range = UniswapV3RangeToken0(self.base_fee_percent, current_price, price_upper, amount)

    def swap(self, price, token0_record, token1_record):
        """
        Process a swap at the given price
        Updates both range positions if they exist
        """
        if self.token0_range is not None:
            self.token0_range.swap(price)
            self.token0_range.settle(token0_record)
        if self.token1_range is not None:
            self.token1_range.swap(price)
            self.token1_range.settle(token1_record)


class Quoter:
    """
    Quoter class for creating tick-aligned ATM positions
    Based on the Skewed_LP_AMM notebook implementation
    """
    def __init__(self, pool, token1_balance, token0_balance, token0_range_pct, token1_range_pct):
        self.pool = pool
        self.balance_token1 = token1_balance  # Quote token (USDC)
        self.balance_token0 = token0_balance  # Base token (ETH)
        self.token0_range_pct = token0_range_pct  # Range percentage for token0 position
        self.token1_range_pct = token1_range_pct  # Range percentage for token1 position
        self.last_quote_price = None
    
    def quote_atm(self, current_price):
        """
        Create tick-aligned ATM positions
        
        Args:
            current_price: Current market price
        """
        # Calculate exact tick for current price
        exact_tick = math.log(current_price) / math.log(1.0001)
        
        # Find tick-aligned boundaries (aligned to 10-tick spacing)
        greatest_lower = math.floor(exact_tick / 10) * 10
        least_upper = math.ceil(exact_tick / 10) * 10
        
        # Convert ticks back to prices
        greatest_lower_price = math.pow(1.0001, greatest_lower)
        least_upper_price = math.pow(1.0001, least_upper)
        
        # Create token1 (USDC) range - BELOW current price
        if self.balance_token1 > 0:
            # Calculate lower bound based on range percentage
            lower_bound_price = greatest_lower_price * (1.0 - self.token1_range_pct)
            # Convert to tick and align
            lower_bound_tick = math.log(lower_bound_price) / math.log(1.0001)
            lower_bound_tick = math.floor(lower_bound_tick / 10) * 10
            lower_bound_price = math.pow(1.0001, lower_bound_tick)
            
            # Create token1 range position
            self.pool.quote_token1(self.balance_token1, greatest_lower_price, lower_bound_price)
            logger.debug(f"Token1 range created: [{lower_bound_price:.2f}, {greatest_lower_price:.2f}]")
        
        # Create token0 (ETH) range - ABOVE current price
        if self.balance_token0 > 0:
            # Calculate upper bound based on range percentage
            upper_bound_price = least_upper_price * (1.0 + self.token0_range_pct)
            # Convert to tick and align
            upper_bound_tick = math.log(upper_bound_price) / math.log(1.0001)
            upper_bound_tick = math.ceil(upper_bound_tick / 10) * 10
            upper_bound_price = math.pow(1.0001, upper_bound_tick)
            
            # Create token0 range position
            self.pool.quote_token0(self.balance_token0, least_upper_price, upper_bound_price)
            logger.debug(f"Token0 range created: [{least_upper_price:.2f}, {upper_bound_price:.2f}]")
        
        self.last_quote_price = current_price


class AMMSimulator:
    """
    AMM Simulator that processes OHLC bars and manages Uniswap V3 positions
    Uses proper Uniswap V3 math for liquidity calculations with tick-aligned positioning
    """

    def __init__(self, fee_tier_bps: int, trade_detection_threshold: float):
        """
        Initialize the AMM Simulator

        Args:
            fee_tier_bps: Fee tier in basis points (e.g., 3000 for 0.3%)
            trade_detection_threshold: Minimum price movement to detect a trade
        """
        self.fee_tier_bps = fee_tier_bps
        self.fee_tier_percent = fee_tier_bps / 10000.0  # Convert bps to percentage
        self.trade_detection_threshold = trade_detection_threshold

        # Initialize Uniswap V3 Pool
        self.pool = UniswapV3Pool(self.fee_tier_percent)

        # Initialize Quoter (will be set when positions are created)
        self.quoter = None

        # Current token balances (updated by swap events)
        self.current_token0_balance = 0.0
        self.current_token1_balance = 0.0

        # Track last price for swap detection
        self.last_price = None

    def compute(self, ohlc_row: pd.Series) -> Optional[SwapEvent]:
        """
        Process single OHLC row and calculate liquidity changes

        Args:
            ohlc_row: Single row from OHLC DataFrame

        Returns:
            SwapEvent if liquidity changes detected, None otherwise
        """
        timestamp = ohlc_row['timestamp']
        close_price = ohlc_row['close']

        # Initialize last price if not set
        if self.last_price is None:
            self.last_price = close_price
            return None

        # Only process if we have active positions
        if not self.has_active_positions():
            self.last_price = close_price
            return None

        # Calculate price movement
        price_change_pct = abs(close_price - self.last_price) / self.last_price

        # Detect significant price movements
        if price_change_pct > self.trade_detection_threshold:
            # Create records for swap
            token0_record = Record()
            token1_record = Record()

            # Process swap through Uniswap V3 pool (this updates position balances)
            self.pool.swap(close_price, token0_record, token1_record)

            # Update balances from records (these are the NEW balances after swap)
            new_token0_balance = token0_record.balance_token0 + token1_record.balance_token0
            new_token1_balance = token0_record.balance_token1 + token1_record.balance_token1

            # Collect fees from both ranges (for event reporting only)
            fees_token0 = token0_record.fees_token0 + token1_record.fees_token0
            fees_token1 = token0_record.fees_token1 + token1_record.fees_token1

            # Calculate trade metrics
            trade_type = 'buy' if close_price > self.last_price else 'sell'
            
            # Calculate liquidity size based on what was actually traded
            sold_token0 = token0_record.sold_token0 + token1_record.sold_token0
            sold_token1 = token0_record.sold_token1 + token1_record.sold_token1
            bought_token0 = token0_record.bought_token0 + token1_record.bought_token0
            bought_token1 = token0_record.bought_token1 + token1_record.bought_token1
            
            if trade_type == 'buy':
                # Price went up - selling token0 for token1
                liquidity_size = sold_token0
                volume_usd = sold_token0 * close_price
            else:
                # Price went down - selling token1 for token0
                liquidity_size = sold_token1
                volume_usd = sold_token1
            
            liquidity_share = price_change_pct

            # Update last price
            self.last_price = close_price

            # Create swap event with updated balances
            swap_event = SwapEvent(
                timestamp=timestamp,
                price=close_price,
                trade_type=trade_type,
                liquidity_size=liquidity_size,
                liquidity_share=liquidity_share,
                fees_token0=fees_token0,
                fees_token1=fees_token1,
                volume_usd=volume_usd,
                new_token0_balance=new_token0_balance,
                new_token1_balance=new_token1_balance
            )

            logger.debug(f"AMM Simulator: {trade_type} at {close_price:.6f} (move: {price_change_pct:.4%}, "
                        f"sold_t0: {sold_token0:.6f}, bought_t1: {bought_token1:.2f}, "
                        f"fees_t0: {fees_token0:.6f}, fees_t1: {fees_token1:.2f})")
            return swap_event

        # Update last price even if no swap occurred
        self.last_price = close_price
        return None


    def get_active_positions_balances(self) -> Tuple[float, float]:
        """
        Get the current token balances from active positions

        Returns:
            Tuple of (token0_balance, token1_balance) from active positions
        """
        token0_balance = 0.0
        token1_balance = 0.0

        # Get balances from V3 pool ranges
        if self.pool.token0_range is not None:
            token0_balance += self.pool.token0_range.balance_token0
            token1_balance += self.pool.token0_range.balance_token1

        if self.pool.token1_range is not None:
            token0_balance += self.pool.token1_range.balance_token0
            token1_balance += self.pool.token1_range.balance_token1

        logger.debug(f"Active positions balances: token0={token0_balance:.6f}, token1={token1_balance:.2f}")
        return token0_balance, token1_balance

    def has_active_positions(self) -> bool:
        """
        Check if there are any active positions

        Returns:
            True if there are active positions, False otherwise
        """
        return self.pool.token0_range is not None or self.pool.token1_range is not None

    def clear_all_positions(self):
        """
        Clear all active positions from the pool
        Used when rebalancing to burn old positions
        """
        self.pool.clear_quote_token0()
        self.pool.clear_quote_token1()
        logger.debug("Cleared all positions from AMM pool")

    def set_initial_balances(self, token0_balance: float, token1_balance: float):
        """
        Set initial token balances

        Args:
            token0_balance: Initial token0 balance
            token1_balance: Initial token1 balance
        """
        self.current_token0_balance = token0_balance
        self.current_token1_balance = token1_balance
        logger.debug(f"AMM Simulator initial balances set: token0={token0_balance:.6f}, token1={token1_balance:.6f}")

    def mint_position(self, token0_amount: float, token1_amount: float,
                     tick_lower: float, tick_upper: float,
                     current_price: float) -> Tuple[float, float, float]:
        """
        Mint a new LP position using tick-aligned Quoter logic

        Args:
            token0_amount: Amount of token0 to deposit
            token1_amount: Amount of token1 to deposit
            tick_lower: Lower price bound (used for range calculation)
            tick_upper: Upper price bound (used for range calculation)
            current_price: Current price (token1/token0)

        Returns:
            Tuple of (liquidity_L, adjusted_token0_amount, adjusted_token1_amount)
        """
        # Calculate range percentages from tick bounds
        token0_range_pct = (tick_upper / current_price - 1) if current_price > 0 else 0.025
        token1_range_pct = (1 - tick_lower / current_price) if current_price > 0 else 0.10
        
        # Create Quoter instance
        self.quoter = Quoter(
            pool=self.pool,
            token1_balance=token1_amount,
            token0_balance=token0_amount,
            token0_range_pct=token0_range_pct,
            token1_range_pct=token1_range_pct
        )
        
        # Create tick-aligned ATM positions
        self.quoter.quote_atm(current_price)
        
        # Calculate total liquidity from both ranges
        liquidity_L = 0.0
        if self.pool.token0_range is not None:
            liquidity_L += self.pool.token0_range.liquidity
        if self.pool.token1_range is not None:
            liquidity_L += self.pool.token1_range.liquidity
        
        # Return the amounts that were actually deployed
        adjusted_token0_amount = token0_amount
        adjusted_token1_amount = token1_amount
        
        logger.debug(f"AMM minted tick-aligned position: L={liquidity_L:.2f}, token0={adjusted_token0_amount:.6f}, token1={adjusted_token1_amount:.6f}")
        
        return liquidity_L, adjusted_token0_amount, adjusted_token1_amount
