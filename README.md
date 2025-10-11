# AsymmetricLP

A Python backtesting engine and trading bot for managing Uniswap V3 liquidity positions with asymmetric ranges and inventory-based rebalancing.

## Features

- **Asymmetric Range Management**: Creates narrow ranges for excess tokens and wide ranges for deficit tokens
- **Uniswap V3 Math**: Full implementation of concentrated liquidity formulas with tick-aligned positioning
- **Inventory-Based Rebalancing**: Automatically rebalances when portfolio deviates from target ratio
- **Historical Backtesting**: Test strategies on real blockchain OHLC data
- **Multiple Pricing Models**: Supports Avellaneda-Stoikov and GLFT models
- **Fee Tracking**: Accurate simulation of LP fee collection

## How it Works

The bot maintains a target inventory ratio (e.g., 50% token0, 50% token1) and creates asymmetric ranges:

**Example**: If you have too much USDC and not enough ETH:
- Creates a **narrow USDC range** above current price → encourages selling USDC to buyers
- Creates a **wide ETH range** below current price → encourages accumulating ETH
- Rebalances when portfolio deviates >10% from target ratio

## Installation

```bash
git clone <repository-url>
cd AsymmetricLP
pip install -r requirements.txt
cp env.example .env
```

## Configuration

Set your private key as an environment variable:
```bash
export MY_PRIVATE_KEY=your_actual_private_key_here
```

Edit `.env` with your RPC URL, token addresses, and chain settings.

## Usage

### Live Trading
```bash
python main.py
```

### Backtesting
```bash
python main.py --historical-mode --ohlc-file data.csv --start-date 2024-01-01 --end-date 2024-01-31
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

### 3-Week Test (December 27, 2023 - January 17, 2024)

**Configuration:**
- Initial Capital: 5,000 USDC + 0.1 ETH (~$5,235 total value @ $2,348/ETH)
- Fee Tier: 0.3% (30 bps)
- Test Period: 21 days
- Data: Real Uniswap V3 OHLC data (4,953 records)
- Rebalance Threshold: 30% inventory deviation
- Pricing Model: Avellaneda-Stoikov

**Results:**

| Metric | Value |
|--------|-------|
| **Initial USDC** | 5,000.00 |
| **Final USDC** | 9,959.06 |
| **USDC Return** | **+99.18%** |
| **Initial ETH** | 0.100 |
| **Final ETH** | 0.183 |
| **ETH Return** | **+83.10%** |
| **Total Trades** | 2,148 |
| **Rebalances** | 12 |
| **Max Drawdown** | 18.81% |

### 3-Day Test (December 27-30, 2023)

| Metric | Value |
|--------|-------|
| **Initial USDC** | 5,000.00 |
| **Final USDC** | 9,983.43 |
| **USDC Return** | **+99.67%** |
| **Initial ETH** | 0.100 |
| **Final ETH** | 0.191 |
| **ETH Return** | **+90.84%** |
| **Total Trades** | 333 |
| **Rebalances** | 9 |
| **Max Drawdown** | 3.75% |

### Key Insights

✅ **Strong Performance**: Both token balances roughly doubled over 3 weeks  
✅ **Balanced Growth**: USDC and ETH returns within 16% of each other  
✅ **Active Trading**: 2,148 swaps executed, generating LP fees  
✅ **Inventory Control**: 12 rebalances maintained target portfolio composition  
✅ **Uniswap V3 Accuracy**: Full implementation of concentrated liquidity math  
✅ **Price Handling**: Correctly uses inverted prices (token1/token0 = ETH/USDC)

**Notes:**
- Returns include all LP fees earned from providing liquidity
- Final balances reflect position values at end of test period
- Backtest uses real on-chain price data from Ethereum mainnet

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.