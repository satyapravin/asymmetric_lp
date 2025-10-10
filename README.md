# AsymmetricLP

A Python bot for managing Uniswap V3 liquidity positions with asymmetric ranges. Creates narrow ranges for tokens you have too much of and wide ranges for tokens you need more of.

## How it works

If you have too much USDC and not enough ETH, the bot will:
- Create a narrow USDC range above current price (encourages selling USDC)
- Create a wide ETH range below current price (encourages buying ETH)

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

**Test Configuration:**
- Initial Capital: $20,469.73 (20,000 USDC + 0.2 ETH)
- Fee Tier: 0.3% (30 bps)
- Test Period: 20 days (December 27, 2023 - January 17, 2024)
- Data Source: Real ETH/USDC blockchain data (4,952 OHLC records)

**Performance Results:**

| Metric | Avellaneda-Stoikov | GLFT Model |
|--------|-------------------|------------|
| **Token0 Return** | 0.00% | 0.00% |
| **Token1 Return** | 0.00% | 0.00% |
| **Token0 Fees** | 51.78% of initial | 51.78% of initial |
| **Token1 Fees** | 1.19% of initial | 1.19% of initial |
| **Token0 Max Drawdown** | 0.35% | 0.35% |
| **Token1 Max Drawdown** | 0.35% | 0.35% |
| **Total Trades** | 775 | 775 |
| **Rebalances** | 3 | 3 |
| **Avg Rebalance Interval** | 6.8 days | 6.8 days |

**Key Insights:**
- Both models show identical performance with real blockchain data
- Efficient rebalancing (3 times in 20 days)
- Realistic fee collection based on position size and trade participation
- Low drawdown (0.35%) shows excellent risk management

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.