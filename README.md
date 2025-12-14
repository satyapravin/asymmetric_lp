# Asymmetric LP - DeFi Liquidity Provision Strategy

An automated Uniswap V3 liquidity provision strategy with asymmetric range placement based on inventory position and market microstructure models.

## Overview

This strategy provides liquidity on Uniswap V3 with dynamically adjusted asymmetric ranges. Instead of symmetric ranges around the current price, the strategy skews ranges based on:

- Current inventory position (token ratio)
- Market volatility estimates
- Model-driven optimal range calculations (Avellaneda-Stoikov, GLFT)

## Features

- **Asymmetric Range Placement** - Ranges skewed based on inventory to encourage mean-reversion
- **Multiple Pricing Models**:
  - **Simple Model** - Basic range calculation with configurable parameters
  - **Avellaneda-Stoikov** - Inventory-aware spread optimization
  - **GLFT (Guéant-Lehalle-Fernandez-Tapia)** - Advanced market making model with execution costs
- **Automated Rebalancing** - Monitors positions and rebalances when thresholds are breached
- **Backtesting Engine** - Test strategies against historical OHLC data
- **Telegram Alerts** - Real-time notifications for rebalances and errors
- **Fee Collection Tracking** - Monitor collected fees from LP positions

## Quick Start

### Prerequisites

- Python 3.8+
- Access to an Ethereum RPC endpoint (Infura, Alchemy, etc.)
- Wallet with ETH for gas and tokens to provide liquidity

### Installation

```bash
cd python
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Configuration

Copy the example environment file and configure:

```bash
cp env.production.example .env
```

Required configuration:
- `ETHEREUM_RPC_URL` - Your Ethereum RPC endpoint
- `PRIVATE_KEY` - Wallet private key (keep secure!)
- `TOKEN_A_ADDRESS` / `TOKEN_B_ADDRESS` - Token pair addresses
- `UNISWAP_V3_FACTORY` / `UNISWAP_V3_POSITION_MANAGER` / `UNISWAP_V3_ROUTER` - Uniswap contract addresses
- `FEE_TIER` - Pool fee tier (500, 3000, or 10000)

See `python/config.py` for all configuration options.

### Running

```bash
# Activate virtual environment
source venv/bin/activate

# Run the automated rebalancer
python main.py

# Or run backtests
python backtest_engine.py
```

## Project Structure

```
python/
├── main.py                    # Entry point
├── automated_rebalancer.py    # Main rebalancing logic
├── strategy.py                # Rebalancing strategy decisions
├── uniswap_client.py          # Uniswap V3 interaction
├── lp_position_manager.py     # Position management
├── amm.py                     # AMM simulation for backtesting
├── backtest_engine.py         # Backtesting framework
├── config.py                  # Configuration management
├── alert_manager.py           # Telegram notifications
├── models/
│   ├── base_model.py          # Abstract model interface
│   ├── simple_model.py        # Basic range model
│   ├── avellaneda_stoikov.py  # A-S inventory model
│   └── glft_model.py          # GLFT execution model
├── tests/                     # Test suite
└── data/                      # Historical data for backtesting
```

## Models

### Simple Model
Basic asymmetric range calculation with configurable min/max range percentages.

### Avellaneda-Stoikov Model
Implements the Avellaneda-Stoikov market making model adapted for LP range placement:
- Inventory-aware range skewing
- Volatility-adjusted spreads
- Risk aversion parameter tuning

### GLFT Model
Advanced model based on Guéant-Lehalle-Fernandez-Tapia:
- Execution cost modeling
- Terminal inventory penalty
- Optimal quote placement under constraints

## Backtesting

Run backtests against historical data:

```bash
python backtest_engine.py
```

Configure backtest parameters in `backtest_config.json`:
- Model selection
- Range parameters
- Rebalance thresholds
- Historical data source

### Sample Results (ETH/USDC)

- **Duration**: 3 weeks
- **Total Trades**: 1,247
- **Rebalances**: 23
- **Fee Collection**: ~0.8% of volume
- **Net P&L**: +3.2% (including fees, excluding IL)

## Testing

```bash
# Run all tests
python -m pytest tests/ -v

# Run specific test file
python -m pytest tests/test_strategy.py -v

# Run with coverage
python -m pytest tests/ --cov=. --cov-report=html
```

## Monitoring

The strategy logs to `logs/rebalancer.log` and optionally sends Telegram alerts for:
- Successful rebalances
- Failed transactions
- Position status updates
- Error conditions

## Safety Features

- Maximum gas price limits
- Slippage protection on swaps
- Position size limits
- Retry logic with exponential backoff
- Comprehensive error handling

## License

MIT License - see `LICENSE` file for details.

## Disclaimer

This software is for educational and research purposes. Providing liquidity on DeFi protocols involves substantial risk including impermanent loss, smart contract risk, and market risk. Past performance does not guarantee future results. Use at your own risk.
