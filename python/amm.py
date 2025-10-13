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
        # Fees: trader pays extra on the token they're giving us
        # We keep that extra as our fee
        self.balance_token0 += -self.sold_token0 + self.bought_token0 + self.fees_token0
        self.balance_token1 += -self.sold_token1 + self.bought_token1 + self.fees_token1
        
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
        self.fees_token0 = 0.0
        self.fees_token1 = 0.0
        # Keep liquidity constant per mint; do not recompute L here

    def compute(self):
        """Compute liquidity from token balances"""
        self.compute_token0_liquidity()
        self.compute_token1_liquidity()

    def compute_token0_liquidity(self):
        """Calculate liquidity from token0 balance"""
        if self.balance_token0 <= 0:
            return
        sL = math.sqrt(self.range_lower)
        sU = math.sqrt(self.range_upper)
        if sU <= sL:
            return
        numerator = self.balance_token0 * sL * sU
        denominator = sU - sL
        L = numerator / denominator
        # L-capping: ensure full-band token0 out does not exceed balance_token0
        denom_cap = (1.0/sL - 1.0/sU)
        if denom_cap > 0:
            Lcap = self.balance_token0 / denom_cap
            L = min(L, Lcap)
        self.liquidity = L

    def compute_token1_liquidity(self):
        """Calculate liquidity from token1 balance"""
        if self.balance_token1 <= 0:
            return
        sU = math.sqrt(self.range_upper)
        sL = math.sqrt(self.range_lower)
        if sU <= sL:
            return
        delta_price = sU - sL
        L = self.balance_token1 / delta_price
        # L-capping: ensure full-band token1 out does not exceed balance_token1
        denom_cap = (sU - sL)
        if denom_cap > 0:
            Lcap = self.balance_token1 / denom_cap
            L = min(L, Lcap)
        self.liquidity = L

    def swap(self, price):
        """
        Process a swap at the given price using Uniswap V3 deltas
        Δtoken0 = L * (1/s1 - 1/s0)
        Δtoken1 = L * (s1 - s0)
        Prices are token1/token0; clip into [range_lower, range_upper].
        """
        if self.range_lower <= 0 or self.range_upper <= 0:
            self.last_spot_price = price
            return
        if self.last_spot_price is None or self.last_spot_price == 0.0:
            self.last_spot_price = price
            return

        # Clip prices into the active range
        s0_price = min(max(self.last_spot_price, self.range_lower), self.range_upper)
        s1_price = min(max(price, self.range_lower), self.range_upper)
        if s0_price == s1_price:
            self.last_spot_price = price
            return

        import math
        s0 = math.sqrt(s0_price)
        s1 = math.sqrt(s1_price)

        # Deltas from pool/LP perspective
        delta_token0 = self.liquidity * (1.0/s1 - 1.0/s0)
        delta_token1 = self.liquidity * (s1 - s0)

        # Reset per-swap counters
        self.sold_token0 = 0.0
        self.bought_token0 = 0.0
        self.sold_token1 = 0.0
        self.bought_token1 = 0.0
        self.fees_token0 = 0.0
        self.fees_token1 = 0.0

        # Up move (s1 > s0): delta1>0, delta0<0 → LP buys token1, sells token0
        if delta_token1 > 0 and delta_token0 < 0:
            sell0 = min(-delta_token0, self.balance_token0)
            ratio = 0.0 if -delta_token0 == 0 else sell0 / (-delta_token0)
            buy1 = delta_token1 * ratio
            self.sold_token0 = sell0
            self.bought_token1 = buy1
            self.fees_token0 = self.sold_token0 * self.fee_tier
        # Down move (s1 < s0): delta1<0, delta0>0 → LP sells token1, buys token0
        elif delta_token1 < 0 and delta_token0 > 0:
            sell1 = min(-delta_token1, self.balance_token1)
            ratio = 0.0 if -delta_token1 == 0 else sell1 / (-delta_token1)
            buy0 = delta_token0 * ratio
            self.sold_token1 = sell1
            self.bought_token0 = buy0
            self.fees_token1 = self.sold_token1 * self.fee_tier
        # Otherwise no action (outside range or zero move after clipping)
        else:
            self.last_spot_price = price
            return

        # Settle into balances
        # balance updates are applied in settle(record) which will be called by pool
        self.last_spot_price = price

    def token0_sold(self, current_price, lower, liquidity):
        """Calculate token0 sold when price moves DOWN (P decreases): sell token0 to buy token1"""
        numerator = math.sqrt(current_price) - math.sqrt(lower)
        denominator = math.sqrt(current_price) * math.sqrt(lower)
        sold_token0 = numerator * liquidity / denominator
        return sold_token0

    def token0_bought(self, current_price, upper, liquidity):
        """Calculate token0 bought when price moves UP (P increases): buy token0 while selling token1"""
        numerator = math.sqrt(upper) - math.sqrt(current_price)
        denominator = math.sqrt(current_price) * math.sqrt(upper)
        bought_token0 = numerator * liquidity / denominator
        return bought_token0

    def token1_sold(self, current_price, upper, liquidity):
        """Calculate token1 sold when price moves UP (P increases): sell token1"""
        return (math.sqrt(upper) - math.sqrt(current_price)) * liquidity

    def token1_bought(self, current_price, lower, liquidity):
        """Calculate token1 bought when price moves DOWN (P decreases): buy token1"""
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
    """Deprecated: use engine-side helpers to compute tick-aligned bands."""
    pass


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
            
            if trade_type == 'buy':
                # Price went up - selling token0 for token1
                liquidity_size = sold_token0
                # token0 is in token0 units (e.g., USD); use as USD volume
                volume_usd = sold_token0
            else:
                # Price went down - selling token1 for token0
                liquidity_size = sold_token1
                # Convert token1 sold into token0 (USD) using 1/price
                volume_usd = sold_token1 * (1.0 / close_price)
            
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

            logger.debug(
                f"AMM Simulator: {trade_type} at {close_price:.6f} (move: {price_change_pct:.4%}, "
                f"sold_t0: {sold_token0:.6f}, sold_t1: {sold_token1:.6f}, "
                f"fees_t0: {fees_token0:.6f}, fees_t1: {fees_token1:.6f})"
            )
            return swap_event

        # DO NOT update last_price if no swap occurred - we need to track actual AMM spot rate
        # If minute 1 had no trade, minute 2 should compare against minute 1's price
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
        Mint a new LP position directly using provided band bounds

        Args:
            token0_amount: Amount of token0 to deposit
            token1_amount: Amount of token1 to deposit
            tick_lower: Lower price bound (used for range calculation)
            tick_upper: Upper price bound (used for range calculation)
            current_price: Current price (token1/token0)

        Returns:
            Tuple of (liquidity_L, adjusted_token0_amount, adjusted_token1_amount)
        """
        # Clear any existing positions for fresh mint
        self.pool.clear_quote_token0()
        self.pool.clear_quote_token1()

        # Mint upper (token0) and lower (token1) bands using provided bounds
        # token0 band active in [current_price, tick_upper]; token1 band active in [tick_lower, current_price]
        if token0_amount and token0_amount > 0:
            # For token0 band, lower bound should be at/near current_price
            self.pool.quote_token0(token0_amount, current_price, tick_upper)
        if token1_amount and token1_amount > 0:
            # For token1 band, upper bound should be at/near current_price
            self.pool.quote_token1(token1_amount, current_price, tick_lower)

        # Calculate total liquidity from both ranges
        liquidity_L = 0.0
        if self.pool.token0_range is not None:
            liquidity_L += self.pool.token0_range.liquidity
        if self.pool.token1_range is not None:
            liquidity_L += self.pool.token1_range.liquidity
        
        # Return the amounts that were actually deployed
        adjusted_token0_amount = token0_amount
        adjusted_token1_amount = token1_amount
        
        logger.debug(f"AMM minted position: L={liquidity_L:.2f}, token0={adjusted_token0_amount:.6f}, token1={adjusted_token1_amount:.6f}, band0=[{current_price:.8f},{tick_upper:.8f}], band1=[{tick_lower:.8f},{current_price:.8f}]")
        
        return liquidity_L, adjusted_token0_amount, adjusted_token1_amount

    def mint_bands_percent(self,
                           current_price: float,
                           range_a_pct: float,
                           range_b_pct: float,
                           token0_amount: float,
                           token1_amount: float) -> Tuple[float, Tuple[float,float], Tuple[float,float]]:
        """
        Compute tick-aligned bands from percent widths and mint positions.
        Args:
            current_price: spot P (token1/token0)
            range_a_pct: upper band percent (token0 side), as fraction (e.g., 0.025)
            range_b_pct: lower band percent (token1 side), as fraction (e.g., 0.10)
            token0_amount: amount to deposit in upper band
            token1_amount: amount to deposit in lower band
        Returns:
            (total_L, (upper_lower, upper_upper), (lower_lower, lower_upper))
        """
        # Raw percentage bands around current price
        upper_lower_raw = current_price
        upper_upper_raw = current_price * (1 + range_a_pct)
        lower_lower_raw = current_price * (1 - range_b_pct)
        lower_upper_raw = current_price

        def price_to_tick(p: float) -> int:
            return math.floor(math.log(p) / math.log(1.0001))

        def tick_to_price(t: int) -> float:
            return math.pow(1.0001, t)

        # Align upper band to ticks
        ul_t = price_to_tick(upper_lower_raw)
        uu_t = price_to_tick(upper_upper_raw)
        if uu_t == ul_t:
            uu_t += 1
        upper_lower = tick_to_price(ul_t)
        upper_upper = tick_to_price(uu_t)

        # Align lower band to ticks
        ll_t = price_to_tick(lower_lower_raw)
        lu_t = price_to_tick(lower_upper_raw)
        if lu_t == ll_t:
            ll_t -= 1
        lower_lower = tick_to_price(ll_t)
        lower_upper = tick_to_price(lu_t)

        # Mint using computed bands
        total_L, _, _ = self.mint_position(
            token0_amount=token0_amount,
            token1_amount=token1_amount,
            tick_lower=lower_lower,
            tick_upper=upper_upper,
            current_price=current_price,
        )
        # Report individual liquidities
        L0 = self.pool.token0_range.liquidity if self.pool.token0_range is not None else 0.0
        L1 = self.pool.token1_range.liquidity if self.pool.token1_range is not None else 0.0
        return total_L, L0, L1, (upper_lower, upper_upper), (lower_lower, lower_upper)
