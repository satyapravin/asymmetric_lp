# AsymmetricLP

A Python backtesting engine and trading bot for managing Uniswap V3 liquidity positions with asymmetric ranges and inventory-based rebalancing.

## Features

- **Asymmetric Range Management**: Creates wide ranges for excess tokens and narrow ranges for deficit tokens
- **Uniswap V3 Math**: Full implementation of concentrated liquidity formulas with tick-aligned positioning
- **Inventory-Based Rebalancing**: Automatically rebalances when portfolio deviates from target ratio
- **Historical Backtesting**: Test strategies on real blockchain OHLC data with accurate P/L calculations
- **Multiple Pricing Models**: Supports Avellaneda-Stoikov and GLFT models with dynamic range sizing
- **Fee Tracking**: Accurate simulation of LP fee collection and portfolio valuation
- **Real Market Data**: Tested on actual Ethereum mainnet OHLC data

## How it Works

- Supports two inventory models: **Avellaneda‑Stoikov** and **GLFT**.
- The target inventory ratio is derived from starting balances and set in config and the model.
- On the very first mint, when deviation ≈ 0, bands are enforced symmetric at `BASE_SPREAD` (tick‑aligned).
- After that, band widths are model‑driven (inventory/volatility terms, execution cost for GLFT) and clamped by min/max range settings.
- Rebalancing is **edge‑triggered** off the last rebalance baselines (price and per‑token balances). Thresholds come from config.
- Rebalances refresh ranges only (`do_conversion=false`) — balances do not change at rebalance time; only swaps move inventory.
- Between rebalances, ranges remain fixed; trades do not invoke the model.
- Prices are handled as token1/token0 (e.g., ETH/USDC); USD valuation uses `value = token0 + token1 / price`.
- Fee tier and trade detection thresholds are configurable (defaults recently tested at 5 bps).

## Installation

```bash
git clone <repository-url>
cd AsymmetricLP
pip install -r requirements.txt
cp env.production.example .env
```

## Configuration

### Production Setup

1. Copy the production example file:
   ```bash
   cp env.production.example .env
   ```

2. Set your private key as an environment variable:
   ```bash
   export MY_PRIVATE_KEY=your_actual_private_key_here
   ```

3. Edit `.env` with your RPC URL, token addresses, and chain settings.

4. See `README_CONFIG.md` for detailed configuration guide and best practices.

### Backtesting Setup

1. Copy the backtest example file:
   ```bash
   cp backtest_config.example.json backtest_config.json
   ```

2. Edit `backtest_config.json` if you need to customize backtest parameters.

**Note**: Backtest parameters are loaded from `backtest_config.json` (separate from `.env`) when running in `--historical-mode`. See `README_CONFIG.md` for details.

## Usage

### Live Trading
```bash
python main.py
```

### Backtesting
```bash
# Basic backtest
python main.py --historical-mode --ohlc-file data.csv --start-date 2024-01-01 --end-date 2024-01-31

# Custom parameters
python main.py --historical-mode \
  --ohlc-file eth_usdc_3weeks_real_fixed.csv \
  --initial-balance-0 5000 \
  --initial-balance-1 0.1 \
  --fee-tier 5
```

### Download Real Data
```bash
python ohlc_downloader.py \
  --token0 0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2 \
  --token1 0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48 \
  --fee 3000 \
  --start-time "2024-01-01 00:00:00" \
  --end-time "2024-01-07 00:00:00" \
  --output eth_usdc_real.csv
```

## Backtest Results

### Latest Results (3-week, ETH/USDC, fee tier 5 bps)

- **GLFT (BASE_SPREAD 15%, REBALANCE_THRESHOLD 40%)**
  - Final balances: token0=3,181.76 USDC, token1=0.73
  - Rebalances: 5; Trades: 2,380; Total return: +3.59%
  - File: `backtest_results_bs15_th40.json`

- **GLFT (BASE_SPREAD 20%, REBALANCE_THRESHOLD 50%)**
  - Final balances: token0=2,479.15 USDC, token1=1.00
  - Rebalances: 3; Trades: 2,380; Total return: +3.47%

- **Avellaneda-Stoikov (BASE_SPREAD 25%, REBALANCE_THRESHOLD 40%)**
  - Final balances: token0=2,329.64 USDC, token1=1.06
  - Rebalances: 3; Trades: 2,380; Total return: +3.47%

Notes:
- Returns are portfolio USD: value = token0 + token1 / price (price = ETH/USDC). Fees are included in balances; gas excluded.
- Wider bands and higher rebalance thresholds (1–2 rebalances/week) reduced churn and improved PnL.

<!-- Old backtest details removed; see Latest Results above for current configuration and outcomes. -->

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.