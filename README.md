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
- Price Movement: ETH declined 7.3% (inverted price: 0.000426 → 0.000395 ETH/USDC)
- Rebalance Threshold: 30% inventory deviation
- Pricing Model: Avellaneda-Stoikov

**Results:**

| Metric | Value |
|--------|-------|
| **Initial USDC** | 5,000.00 |
| **Final USDC** | 5,016.16 |
| **USDC Return** | **+0.32%** |
| **Initial ETH** | 0.100 |
| **Final ETH** | 0.083 |
| **ETH Return** | **-17.0%** |
| **Initial Value** | ~$5,235 |
| **Final Value** | ~$5,211 |
| **Total Return** | **-0.46%** |
| **Total Trades** | 2,148 |
| **Rebalances** | 12 |

### 3-Day Test (December 27-30, 2023)

| Metric | Value |
|--------|-------|
| **Initial USDC** | 5,000.00 |
| **Final USDC** | 5,009.01 |
| **USDC Return** | **+0.18%** |
| **Initial ETH** | 0.100 |
| **Final ETH** | 0.092 |
| **ETH Return** | **-8.0%** |
| **Initial Value** | ~$5,235 |
| **Final Value** | ~$5,225 |
| **Total Return** | **-0.19%** |
| **Total Trades** | 333 |
| **Rebalances** | 9 |

### Key Insights

✅ **Capital Preservation**: Near-zero total returns despite 7% ETH price decline  
✅ **LP Fee Generation**: Small USDC gains from providing liquidity  
✅ **Impermanent Loss**: ETH balance declined as price fell (rebalancing effect)  
✅ **Active Market Making**: 2,148 trades over 3 weeks, 333 in 3 days  
✅ **Frequent Rebalancing**: 12 rebalances maintained inventory targets  
✅ **Uniswap V3 Math**: Accurate concentrated liquidity implementation  

**Understanding the Results:**
- LP strategies naturally accumulate more of the deprecating asset (ETH in this case)
- USDC gains (+0.32%) come from LP fees earned on 2,148 trades
- ETH losses (-17%) reflect both price impact and rebalancing dynamics
- Near-zero total return shows capital preservation in declining market
- Real-world test on actual Ethereum mainnet price data

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.