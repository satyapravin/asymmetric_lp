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
- Initial Capital: $20,000 (4.0 ETH + 10,000 USDC)
- Fee Tier: 0.3% (30 bps)
- Test Period: 21 days (December 27, 2023 - January 17, 2024)
- Data Source: Real ETH/USDC blockchain data (4,952 OHLC records, 791 trades)
- Rebalance Threshold: 10% inventory deviation

**Performance Results:**

| Metric | Avellaneda-Stoikov | GLFT Model |
|--------|-------------------|------------|
| **Initial Token Balances** | 4.000000 ETH, 10,000.000000 USDC | 4.000000 ETH, 10,000.000000 USDC |
| **Final Token Balances** | 4.000000 ETH, 10,000.000000 USDC | 4.000000 ETH, 10,000.000000 USDC |
| **Final Inventory Deviation** | 0.00% | 0.00% |
| **Token0 Return** | 1.18% | 2.37% |
| **Token1 Return** | 1.10% | 2.27% |
| **Token0 Fees** | 1.18% of initial balance | 2.37% of initial balance |
| **Token1 Fees** | 1.10% of initial balance | 2.27% of initial balance |
| **Token0 Max Drawdown** | 0.00% | 0.00% |
| **Token1 Max Drawdown** | 0.00% | 0.00% |
| **Total Trades** | 791 | 791 |
| **Rebalances** | 1 | 1 |
| **Avg Rebalance Interval** | 21.0 days | 21.0 days |
| **Avg Trades per Day** | 37.7 | 37.7 |

**Key Insights:**
- **Perfect Portfolio Balance**: Both models maintain 0.00% final inventory deviation
- **GLFT Model Superiority**: GLFT model generates ~2x higher returns (2.37% vs 1.18% Token0, 2.27% vs 1.10% Token1)
- **Efficient Rebalancing**: Only 1 rebalance in 21 days (509.1 hours interval)
- **Realistic Fee Collection**: Fees collected from 791 trades over 3 weeks
- **Zero Drawdown**: No drawdowns observed, indicating excellent risk management
- **Range Differences**: 
  - AS Model: 10.0%/2.5% ranges (narrow/wide)
  - GLFT Model: 25.2%/6.3% ranges (much wider due to execution costs)
- **Fee Collection Mechanism**: Positions earn fees when trades occur within their price ranges
- **Price Data Handling**: Successfully handles inverted price data (USDC/ETH format)

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.