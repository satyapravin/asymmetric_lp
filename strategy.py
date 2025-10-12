"""
Shared strategy logic for AsymmetricLP.
Centralizes rebalance triggers and range sizing so both live and backtests use the same rules.
"""
from typing import Dict, Any, Tuple, Optional, List
import math


class AsymmetricLPStrategy:
    """
    Encapsulates business logic for when to rebalance and how to size ranges.
    This class is deliberately free of side-effects (no AMM minting, no IO).
    """
    def __init__(self, config, inventory_model, token_a_address: Optional[str] = None, token_b_address: Optional[str] = None):
        self.config = config
        self.inventory_model = inventory_model
        self.token_a_address = token_a_address
        self.token_b_address = token_b_address

    def should_rebalance(
        self,
        current_price: float,
        price_history: List[Dict[str, float]],
        initial_token0: float,
        initial_token1: float,
        current_token0: float,
        current_token1: float,
    ) -> bool:
        """
        Per-token depletion trigger used by both backtest and live.
        Rebalance if either token is depleted by more than REBALANCE_THRESHOLD.
        """
        if current_price is None:
            return False
        # Always allow first mint handled by caller (no positions yet)
        init0 = max(initial_token0, 1e-18)
        init1 = max(initial_token1, 1e-18)
        cur0 = max(current_token0, 0.0)
        cur1 = max(current_token1, 0.0)
        dev0 = (init0 - cur0) / init0 if cur0 < init0 else 0.0
        dev1 = (init1 - cur1) / init1 if cur1 < init1 else 0.0
        return (dev0 > self.config.REBALANCE_THRESHOLD) or (dev1 > self.config.REBALANCE_THRESHOLD)

    def plan_rebalance(
        self,
        current_price: float,
        price_history: List[Dict[str, float]],
        token0_balance: float,
        token1_balance: float,
        initial_target_ratio: float,
        startup_allocation: bool,
        client: Optional[Any] = None,
        token_a_address: Optional[str] = None,
        token_b_address: Optional[str] = None,
        initial_token0_units: Optional[float] = None,
        initial_token1_units: Optional[float] = None,
        do_conversion: bool = True,
    ) -> Tuple[float, float, float, float, Dict[str, Any]]:
        """
        Compute model-driven ranges and adjust balances toward target ratio.

        Returns:
            range_a_pct, range_b_pct, adjusted_token0_balance, adjusted_token1_balance, ranges_raw
        """
        # Convert to wei for the model
        token0_balance_wei = int(token0_balance * 10**18)
        token1_balance_wei = int(token1_balance * 10**18)

        # Default mock client with 18 decimals if none supplied
        if client is None:
            class _MockClient:
                def get_token_decimals(self, address):
                    return 18
            client = _MockClient()

        # Prefer explicit addresses passed in, else use configured ones, else dummy
        token_a = token_a_address or self.token_a_address or "0x0000000000000000000000000000000000000000"
        token_b = token_b_address or self.token_b_address or "0x0000000000000000000000000000000000000001"

        ranges = self.inventory_model.calculate_lp_ranges(
            token0_balance=token0_balance_wei,
            token1_balance=token1_balance_wei,
            spot_price=current_price,
            price_history=price_history,
            token_a_address=token_a,
            token_b_address=token_b,
            client=client,
        )

        range_a_pct = ranges['range_a_percentage'] / 100.0
        range_b_pct = ranges['range_b_percentage'] / 100.0

        # Adjust balances toward target only after startup mint
        adj_t0 = token0_balance
        adj_t1 = token1_balance
        if not startup_allocation and do_conversion:
            # If initial token unit targets provided, aim to restore those units
            if initial_token0_units is not None and initial_token1_units is not None:
                # Bring token0 toward initial units using available token1
                if adj_t0 > initial_token0_units:
                    token0_excess_units = adj_t0 - initial_token0_units
                    token1_to_buy = token0_excess_units * current_price
                    sell_cap = min(token1_to_buy, adj_t1)
                    adj_t0 -= sell_cap / current_price
                    adj_t1 += sell_cap
                elif adj_t0 < initial_token0_units:
                    token0_deficit_units = initial_token0_units - adj_t0
                    token1_to_sell = min(token0_deficit_units * current_price, adj_t1)
                    adj_t0 += token1_to_sell / current_price
                    adj_t1 -= token1_to_sell
                # Then adjust token1 toward initial units if possible using token0
                if adj_t1 > initial_token1_units:
                    token1_excess_units = adj_t1 - initial_token1_units
                    token0_to_buy = token1_excess_units / current_price
                    sell_cap0 = min(token0_to_buy, adj_t0)
                    adj_t1 -= sell_cap0 * current_price
                    adj_t0 += sell_cap0
                elif adj_t1 < initial_token1_units:
                    token1_deficit_units = initial_token1_units - adj_t1
                    token0_to_sell = min(token1_deficit_units / current_price, adj_t0)
                    adj_t1 += token0_to_sell * current_price
                    adj_t0 -= token0_to_sell
            else:
                # Fallback to USD-value ratio targeting if units not provided
                current_value_0 = token0_balance
                current_value_1 = token1_balance * (1.0 / current_price)
                total_value_usd = current_value_0 + current_value_1
                target_token0_value = total_value_usd * initial_target_ratio
                target_token1_value = total_value_usd * (1.0 - initial_target_ratio)
                token0_excess = current_value_0 - target_token0_value
                token1_excess = current_value_1 - target_token1_value
                if token0_excess > 0:
                    token0_to_sell = min(token0_excess, adj_t0)
                    token1_to_buy = token0_to_sell * current_price
                    adj_t0 -= token0_to_sell
                    adj_t1 += token1_to_buy
                elif token1_excess > 0:
                    token1_to_sell = min(token1_excess * current_price, adj_t1)
                    token0_to_buy = token1_to_sell / current_price
                    adj_t0 += token0_to_buy
                    adj_t1 -= token1_to_sell

        # No allocation bias here: keep model widths intact and deploy current balances
        # (conversion may have adjusted adj_t0/adj_t1 during startup)

        return range_a_pct, range_b_pct, max(adj_t0, 0.0), max(adj_t1, 0.0), ranges


