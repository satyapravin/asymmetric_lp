# Configuration Guide

This document explains how to configure the AsymmetricLP system for production and backtesting.

## Configuration Files

### Production Configuration

**File**: `.env` (copy from `env.production.example`)

This is the main configuration file for live trading. It contains:
- Blockchain connection settings (RPC URL, chain ID)
- Wallet private key (via environment variable reference)
- Uniswap V3 contract addresses
- Token pair configuration
- Strategy parameters (model, ranges, thresholds)
- Optional integrations (ZMQ, Telegram)

**Important**: Never commit `.env` files with real private keys to version control!

### Backtesting Configuration

**File**: `backtest_config.json` (copy from `backtest_config.example.json`)

This is a **separate JSON file** (not an environment file) that contains parameters **only used for backtesting**:
- `risk_free_rate` - Annual risk-free rate for performance calculations
- `default_daily_volatility` - Default volatility when insufficient price history
- `default_initial_balance_0` - Starting balance for token A in backtests
- `default_initial_balance_1` - Starting balance for token B in backtests
- `trade_detection_threshold` - Threshold for detecting trades in OHLC data
- `position_falloff_factor` - Position valuation falloff factor

**Important**: Backtest parameters are **NOT** loaded from `.env` - they come exclusively from `backtest_config.json`.

## Setup Instructions

### For Production (Live Trading)

1. Copy the production example file:
   ```bash
   cp env.production.example .env
   ```

2. Edit `.env` and fill in:
   - Your Ethereum RPC URL
   - Private key reference (e.g., `PRIVATE_KEY=${MY_PRIVATE_KEY}`)
   - Token addresses for your trading pair
   - Strategy parameters (use best backtest results as starting point)

3. Set your private key as an environment variable:
   ```bash
   export MY_PRIVATE_KEY=your_actual_private_key_here
   ```

4. Validate configuration:
   ```bash
   python -c "from config import Config; Config.validate_config()"
   ```

5. Run live trading:
   ```bash
   python main.py
   ```

### For Backtesting

1. Copy the backtest example file:
   ```bash
   cp backtest_config.example.json backtest_config.json
   ```

2. Edit `backtest_config.json` and adjust backtest parameters if needed

3. Run backtest:
   ```bash
   python main.py --historical-mode --ohlc-file data/eth_usdc_3weeks_real_fixed.csv
   ```

The system will automatically:
- Load `.env` for production parameters (RPC, contracts, strategy params)
- Load `backtest_config.json` for backtesting parameters (simulation settings)
- These are completely separate - backtest params never come from `.env`

## Best Production Configuration

Based on backtest results, the best performing configuration is:

```ini
# Model configuration
INVENTORY_MODEL=GLFTModel

# Rebalancing configuration
MIN_RANGE_PERCENTAGE=10.0
MAX_RANGE_PERCENTAGE=50.0
REBALANCE_THRESHOLD=0.30

# Strategy parameters
BASE_SPREAD=0.20
INVENTORY_RISK_AVERSION=0.1
TARGET_INVENTORY_RATIO=0.5
MAX_INVENTORY_DEVIATION=0.3

# GLFT model parameters
EXECUTION_COST=0.001
INVENTORY_PENALTY=0.05
MAX_POSITION_SIZE=0.1
TERMINAL_INVENTORY_PENALTY=0.2
INVENTORY_CONSTRAINT_ACTIVE=true
```

This configuration achieved **3.46% return** with only **5 rebalances** over 3 weeks.

## Parameter Reference

### Production Parameters

| Parameter | Description | Default | Required |
|-----------|-------------|---------|----------|
| `ETHEREUM_RPC_URL` | Blockchain RPC endpoint | - | Yes |
| `PRIVATE_KEY` | Wallet private key (env var ref) | - | Yes |
| `CHAIN_ID` | Blockchain network ID | 1 | Yes |
| `TOKEN_A_ADDRESS` | Token A contract address | - | Yes |
| `TOKEN_B_ADDRESS` | Token B contract address | - | Yes |
| `FEE_TIER` | Uniswap V3 fee tier (5/500/3000/10000) | 5 | Yes |
| `INVENTORY_MODEL` | Model: `AvellanedaStoikovModel` or `GLFTModel` | `AvellanedaStoikovModel` | No |
| `MIN_RANGE_PERCENTAGE` | Minimum LP range width (%) | 2.0 | No |
| `MAX_RANGE_PERCENTAGE` | Maximum LP range width (%) | 50.0 | No |
| `REBALANCE_THRESHOLD` | Inventory deviation threshold | 0.10 | No |
| `BASE_SPREAD` | Base spread for range calculation | 0.20 | No |
| `ZMQ_ENABLED` | Enable ZeroMQ publishing | false | No |

### Backtesting Parameters (`backtest_config.json`)

| Parameter | Description | Default |
|-----------|-------------|---------|
| `risk_free_rate` | Annual risk-free rate | 0.02 |
| `default_daily_volatility` | Default volatility | 0.02 |
| `default_initial_balance_0` | Starting token A balance | 3000.0 |
| `default_initial_balance_1` | Starting token B balance | 1.0 |
| `trade_detection_threshold` | Trade detection threshold | 0.0005 |
| `position_falloff_factor` | Position falloff factor | 0.1 |

## Environment Variable Reference Format

For sensitive values like private keys, use environment variable references:

```bash
# In .env file:
PRIVATE_KEY=${MY_PRIVATE_KEY}

# In shell:
export MY_PRIVATE_KEY=0x1234...
```

This ensures private keys are never stored in files.

## Troubleshooting

### Configuration Not Loading

- Ensure `.env` file exists in the `python/` directory
- Check file permissions (should be readable)
- Verify environment variable references are correct

### Backtest Parameters Not Applied

- Ensure `backtest_config.json` exists (or `backtest_config.example.json` as fallback)
- Check JSON syntax is valid
- Verify parameter names match exactly (lowercase with underscores)

### Validation Errors

Run validation to see specific errors:
```bash
python -c "from config import Config; Config.validate_config()"
```

