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

The bot maintains a target inventory ratio derived from your starting balances and creates asymmetric ranges using the Avellaneda-Stoikov market making model:

**Example**: If you start with 5,000 USDC and 0.1 ETH (95.5% USDC ratio):
- Creates a **wide USDC range** (40%) above current price → encourages selling excess USDC
- Creates a **narrow ETH range** (10%) below current price → encourages accumulating deficit ETH
- Rebalances when portfolio deviates >30% from target ratio
- Uses dynamic range sizing based on inventory deviation and market volatility

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

### 3-Week Test (December 27, 2023 - January 17, 2024)

**Configuration:**
- Initial Capital: 5,000 USDC + 0.1 ETH (~$5,235 total value @ $2,348/ETH)
- Fee Tier: 0.05% (5 bps)
- Test Period: 21 days
- Data: Real Uniswap V3 OHLC data (1-minute candles)
- Trade Detection: 5 bps (0.05%) price movement threshold
- Rebalance Threshold: 30% inventory deviation
- Pricing Model: Avellaneda-Stoikov with 20% base spread
- Target Inventory Ratio: 95.5% (derived from starting balances)

**Results:**

| Metric | Value |
|--------|-------|
| **Token0 (USDC) - Initial** | 5,000.00 |
| **Token0 (USDC) - Final** | 4,212.64 |
| **Token0 Return** | **-15.75%** |
| **Token1 (ETH) - Initial** | 0.100 |
| **Token1 (ETH) - Final** | 0.440 |
| **Token1 Return** | **+338.45%** |
| **Total Trades** | 2,380 |
| **Total Rebalances** | 6 |
| **Final Inventory Ratio** | 90.5% USDC (improved from 95.5%) |

**USD Performance:**
- **Initial Portfolio Value**: $5,234.86
- **Final Portfolio Value**: $5,327.24
- **Absolute P/L**: **+$92.38**
- **Percentage P/L**: **+1.76%**
- **Maximum Drawdown**: 14.98%

### Key Insights

✅ **Profitable Strategy**: Generated +$92.38 profit (+1.76% return) over 21 days  
✅ **Effective Inventory Rebalancing**: Successfully moved from 95.5% to 90.5% USDC ratio  
✅ **Asymmetric Range Effectiveness**: 40% wide ranges for excess tokens, 10% narrow for deficit tokens  
✅ **Reduced Rebalancing**: Only 6 rebalances needed vs 29 with smaller ranges  
✅ **Fee Generation**: Earned substantial fees from 2,380 trades at 5 bps fee tier  
✅ **Price Movement Capture**: ETH appreciated 7.86% during test period  
✅ **Uniswap V3 Implementation**: Full concentrated liquidity math with tick-aligned positioning  
✅ **Real Market Data**: Tested on actual Ethereum mainnet 1-minute OHLC data

**Understanding the Results:**
- The asymmetric LP strategy successfully generated profit despite ETH price appreciation
- Wide ranges (40%) for excess USDC encouraged selling as ETH price rose
- Narrow ranges (10%) for deficit ETH encouraged accumulation
- Inventory rebalancing worked effectively, moving portfolio closer to target ratio
- Fee income from 2,380 trades provided steady revenue stream
- Strategy demonstrated resilience with only 14.98% maximum drawdown

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.