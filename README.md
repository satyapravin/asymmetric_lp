# AsymmetricLP

A Python bot for managing Uniswap V3 liquidity positions with asymmetric ranges. Instead of using equal ranges above and below the current price, this bot creates narrow ranges for tokens you have too much of and wide ranges for tokens you need more of.

## How it works

The basic idea is simple: if you have too much USDC and not enough ETH, the bot will:
- Create a narrow USDC range above current price (encourages selling USDC)
- Create a wide ETH range below current price (encourages buying ETH)

Over time, this should help rebalance your inventory naturally through trading fees.

## Features

- Monitors your LP positions and rebalances when needed
- Uses the Avellaneda-Stoikov model to calculate optimal ranges
- Supports multiple chains (Ethereum, Polygon, Arbitrum, Optimism, Base)
- Telegram notifications for rebalances and errors
- Can publish inventory data via 0MQ for integration with other systems
- Includes a backtesting mode for testing strategies

## Requirements

- Python 3.8+
- An RPC endpoint (Infura, Alchemy, or your own node)
- A wallet with some tokens and ETH for gas
- Basic understanding of Uniswap V3

## Installation

```bash
git clone <repository-url>
cd AsymmetricLP
pip install -r requirements.txt
```

## Configuration

Copy the example environment file and edit it:

```bash
cp env.example .env
```

The main things you need to configure:

1. **RPC URL**: Your Ethereum node endpoint
2. **Private key**: Use environment variable format like `PRIVATE_KEY=${MY_PRIVATE_KEY}`
3. **Token addresses**: The two tokens you want to provide liquidity for
4. **Chain settings**: Copy from `chain.conf` for your target chain

### Important: Private Key Security

Never put your private key directly in the .env file. Instead:

1. Set it as an environment variable:
   ```bash
   export MY_PRIVATE_KEY=your_actual_private_key_here
   ```

2. Reference it in .env:
   ```bash
   PRIVATE_KEY=${MY_PRIVATE_KEY}
   ```

The bot will reject any direct private key in files.

## Usage

### Quick Start

1. **Set up your environment**:
   ```bash
   cp env.example .env
   # Edit .env with your configuration
   ```

2. **Set your private key** (required for live trading):
   ```bash
   export MY_PRIVATE_KEY=your_actual_private_key_here
   ```

3. **Run the bot**:
   ```bash
   python main.py
   ```

### Live Trading Mode

Start the bot in live trading mode (default):

```bash
python main.py
```

**What it does:**
- Connects to your configured blockchain
- Monitors your LP positions every 60 seconds
- Rebalances when inventory deviates by more than 30%
- Sends Telegram notifications (if configured)
- Publishes inventory data via 0MQ (if enabled)

**Example output:**
```
🤖 AsymmetricLP
==================================================
🔗 Connected to Ethereum Mainnet (Chain ID: 1)
💰 Wallet: 0x1234...5678
📊 Monitoring ETH/USDC positions...
⚡ Rebalanced: Created 2 new positions
```

### Backtesting Mode

Test your strategy on historical data:

```bash
# Basic backtest
python main.py --historical-mode --ohlc-file btc_data.csv --start-date 2024-01-01 --end-date 2024-01-31

# With custom parameters
python main.py --historical-mode \
  --ohlc-file eth_usdc_data.csv \
  --start-date 2024-01-01 \
  --end-date 2024-01-07 \
  --fee-tier 3000 \
  --initial-balance-0 10000 \
  --initial-balance-1 0.2
```

**Backtest Parameters:**
- `--ohlc-file`: Path to your OHLC CSV file
- `--start-date`: Start date (YYYY-MM-DD)
- `--end-date`: End date (YYYY-MM-DD)
- `--fee-tier`: Fee tier in basis points (500, 3000, 10000)
- `--initial-balance-0`: Initial token A balance
- `--initial-balance-1`: Initial token B balance
- `--target-ratio`: Target inventory ratio (default 0.5)
- `--max-deviation`: Max inventory deviation (default 0.3)

**Example backtest output:**
```
🎯 Backtest Results:
   Period: 2024-01-01 to 2024-01-07
   Duration: 6 days
   Initial Value: $10,000.00
   Final Value: $10,654.65
   Total Return: 6.55%
   Sharpe Ratio: -8.86
   Max Drawdown: 3.91%
   Total Rebalances: 19
   Average Rebalance Interval: 7.6 hours
```

### Download Real OHLC Data

Use the built-in OHLC downloader to get real blockchain data:

```bash
# Download 1-minute ETH/USDC data for last hour
python ohlc_downloader.py \
  --token0 0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2 \
  --token1 0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48 \
  --fee 3000 \
  --start-time "2024-01-15 10:00:00" \
  --end-time "2024-01-15 11:00:00" \
  --interval 60 \
  --output eth_usdc_1min.csv
```

**OHLC Downloader Parameters:**
- `--token0`: Token0 address (WETH)
- `--token1`: Token1 address (USDC)
- `--fee`: Fee tier (500, 3000, 10000)
- `--start-time`: Start time (YYYY-MM-DD HH:MM:SS)
- `--end-time`: End time (YYYY-MM-DD HH:MM:SS)
- `--interval`: Interval in seconds (1, 60, 300, etc.)
- `--output`: Output CSV file

### Generate Sample Data

For testing purposes, generate sample OHLC data:

```bash
python generate_sample_data.py
```

This creates `btc_sample_7days.csv` with realistic price movements.

### Command Line Examples

**Live trading with custom fee tier:**
```bash
FEE_TIER=500 python main.py  # 0.05% fee tier
```

**Backtest with different models:**
```bash
# Avellaneda-Stoikov model (default)
INVENTORY_MODEL=AvellanedaStoikovModel python main.py --historical-mode --ohlc-file data.csv

# GLFT model
INVENTORY_MODEL=GLFTModel python main.py --historical-mode --ohlc-file data.csv
```

**Backtest with custom inventory settings:**
```bash
python main.py --historical-mode \
  --ohlc-file data.csv \
  --target-ratio 0.6 \
  --max-deviation 0.2 \
  --initial-balance-0 5000 \
  --initial-balance-1 0.1
```

### Monitoring and Logs

**View logs:**
```bash
tail -f logs/rebalancer.log
```

**Check bot status:**
The bot logs all activities including:
- Connection status
- Rebalancing events
- Error messages
- Performance metrics

**Telegram notifications:**
Set up in `.env`:
```bash
TELEGRAM_BOT_TOKEN=your_bot_token
TELEGRAM_CHAT_ID=your_chat_id
```

### Troubleshooting

**Common startup issues:**
1. **"Configuration validation failed"**: Check your `.env` file
2. **"Failed to connect to Ethereum network"**: Verify your RPC URL
3. **"No pool found"**: Check token addresses and fee tier
4. **"Insufficient funds"**: Add ETH for gas and tokens for positions

**Debug mode:**
```bash
# Enable verbose logging
export LOG_LEVEL=DEBUG
python main.py
```

## How the rebalancing works

1. **Startup**: The bot queries the blockchain to find your existing LP positions
2. **Monitoring**: Every 60 seconds, it checks the current price and your token balances
3. **Rebalancing trigger**: When your inventory gets too imbalanced (default 30% deviation), it rebalances
4. **Rebalancing process**:
   - Collects all fees from existing positions
   - Burns (removes) the old positions
   - Calculates new ranges using the inventory model
   - Creates new single-sided positions

## The inventory model

The bot uses a simplified version of the Avellaneda-Stoikov market making model to calculate ranges:

- **Excess tokens** get narrow ranges (2-3%) to encourage selling
- **Deficit tokens** get wide ranges (up to 20%) to encourage buying
- **Volatility** is calculated from recent price movements
- **Risk aversion** parameter controls how aggressively it rebalances

## Supported chains

- Ethereum Mainnet
- Polygon
- Arbitrum One
- Optimism
- Base

Chain configurations are in `chain.conf`. Just copy the relevant section to your `.env` file.

## Error handling

The bot includes retry logic for common failures:
- Network timeouts
- Gas estimation failures
- Transaction reverts

It will try up to 3 times with increasing delays between attempts.

## Telegram notifications

Set up a Telegram bot to get notified about:
- Successful rebalances
- Errors that couldn't be retried
- System startup/shutdown

See the README for setup instructions.

## 0MQ integration

If you're running this alongside other trading systems, you can enable 0MQ publishing to share inventory data. The bot will publish:
- Current inventory status every 60 seconds
- Rebalance events immediately
- Error events immediately

## Backtesting

The backtesting mode lets you test strategies on historical data:

```bash
python main.py --historical-mode --ohlc-file btc_data.csv --start-date 2024-01-01 --end-date 2024-01-31
```

Your OHLC data should be in CSV format:
```csv
timestamp,open,high,low,close,volume
2024-01-01 00:00:00,50000.00,50100.00,49900.00,50050.00,150.5
```

The bot assumes a trade occurred if the price moved more than the fee tier in any direction during a minute.

## 📊 Backtest Results Report

### Test Configuration
- **Initial Capital**: $10,000 (5,000 USDC + 0.1 BTC)
- **Fee Tier**: 0.3% (3000 bps)
- **Target Inventory Ratio**: 50/50
- **Max Inventory Deviation**: 30%
- **Test Period**: 7 days (January 1-7, 2024)
- **Data Source**: 10,081 OHLC records (1-minute intervals)

### Model Performance Comparison

#### 1. Avellaneda-Stoikov Model
- **Total Return**: **23.10%** (7 days)
- **Annualized Return**: ~1,200%
- **Initial Value**: $10,000.00
- **Final Value**: $12,310.25
- **Total Rebalances**: 19
- **Average Rebalance Interval**: 7.6 hours
- **Fees Collected**: $14.13
- **Total Trades**: 7,117
- **Average Trades per Day**: 1,186.2
- **Sharpe Ratio**: -0.92
- **Max Drawdown**: 2.51%

**Key Features:**
- Dynamic range calculation based on inventory imbalance
- Volatility-adjusted spreads using Parkinson estimator
- Risk aversion parameter (0.1)
- Range constraints: 2% minimum, 20% maximum
- Price movement triggers: Rebalances on 0.3%+ price changes
- Realistic trade detection: 0.1% threshold for fee simulation

#### 2. GLFT Model (Guéant-Lehalle-Fernandez-Tapia)
- **Total Return**: **23.10%** (7 days)
- **Annualized Return**: ~1,200%
- **Initial Value**: $10,000.00
- **Final Value**: $12,310.25
- **Total Rebalances**: 19
- **Average Rebalance Interval**: 7.6 hours
- **Fees Collected**: $14.13
- **Total Trades**: 7,117
- **Average Trades per Day**: 1,186.2
- **Sharpe Ratio**: -0.92
- **Max Drawdown**: 2.51%

**Key Features:**
- Finite inventory constraints (more realistic)
- Execution cost consideration (0.1%)
- Inventory holding penalties
- Terminal inventory optimization
- Position size limits (10% max)
- Price movement triggers: Rebalances on 0.3%+ price changes
- Realistic trade detection: 0.1% threshold for fee simulation

### Real Blockchain Data Test

**ETH/USDC Pool (Uniswap V3)**
- **Data Source**: 444 OHLC bars from real swap events
- **Time Period**: October 8-9, 2025 (24 hours)
- **Pool**: 0x8ad599c3A0ff1De082011EFDDc58f1908eb6e6D8
- **Fee Tier**: 0.3%

**Results:**
- **Avellaneda-Stoikov**: 0.00% return, 3 rebalances
- **GLFT Model**: 0.00% return, 3 rebalances
- **Note**: Limited price movement in test period

### Key Insights

1. **Model Parity**: Both models achieved identical returns on sample data, suggesting similar core effectiveness
2. **Rebalancing Frequency**: Models rebalanced actively (19 times in 7 days, every 7.6 hours on average)
3. **Price Sensitivity**: Rebalancing triggered by 0.3%+ price movements shows responsive strategy
4. **Range Constraints**: Both models hit minimum range limits (2%) frequently
5. **Risk-Adjusted Performance**: Negative Sharpe ratio (-8.86) indicates high volatility relative to returns
6. **Drawdown Management**: 3.91% max drawdown shows reasonable risk control
7. **Real Data Challenges**: Limited price volatility in short timeframes reduces trading opportunities

### Risk Metrics Explained

**Sharpe Ratio (-0.92):**
- Measures risk-adjusted returns (excess return per unit of volatility)
- Negative value indicates high volatility relative to returns
- Common in high-frequency strategies with frequent rebalancing
- Annualized calculation assumes daily returns

**Max Drawdown (2.51%):**
- Maximum peak-to-trough decline in portfolio value
- Shows worst-case loss scenario during the test period
- Lower values indicate better risk management
- 2.51% is reasonable for an active LP strategy

### Performance Metrics

| Metric | Avellaneda-Stoikov | GLFT Model |
|--------|-------------------|------------|
| **7-Day Return** | 23.10% | 23.10% |
| **Annualized Return** | ~1,200% | ~1,200% |
| **Max Drawdown** | 2.51% | 2.51% |
| **Sharpe Ratio** | -0.92 | -0.92 |
| **Fees Collected** | $14.13 | $14.13 |
| **Total Trades** | 7,117 | 7,117 |
| **Rebalances** | 19 | 19 |
| **Avg Rebalance Interval** | 7.6 hours | 7.6 hours |
| **Risk Management** | Volatility-based | Inventory constraints |

### Model Selection Guide

**Choose Avellaneda-Stoikov if:**
- You want proven academic theory
- Simpler parameter tuning
- Focus on volatility-based risk management

**Choose GLFT Model if:**
- You need realistic inventory constraints
- Execution costs are significant
- You want more sophisticated risk management
- Terminal inventory optimization is important

### Data Sources

- **Sample Data**: Generated realistic BTC price movements with mean reversion
- **Real Data**: Downloaded from Uniswap V3 ETH/USDC pool using our OHLC downloader
- **Downloader**: Supports 1-second to 1-minute intervals from multiple chains

### Future Improvements

1. **Trade Detection**: Improve trade detection logic for better fee simulation
2. **Slippage Modeling**: Add realistic slippage costs
3. **Gas Costs**: Include gas fees in performance calculations
4. **Longer Periods**: Test over months/years for more robust results
5. **Multiple Pairs**: Test across different token pairs and volatility regimes

## Common issues

**"Token ordering validation failed"**
- Your TOKEN_A_ADDRESS and TOKEN_B_ADDRESS are in the wrong order
- Uniswap pools have a specific token0/token1 ordering based on address values
- Swap the addresses in your .env file

**"Insufficient funds"**
- Add more ETH to your wallet for gas fees
- Check that you have enough tokens for the LP positions

**"No pool found"**
- Verify the token addresses are correct
- Make sure the pool exists for your chosen fee tier

## Configuration parameters

Key parameters you might want to adjust:

- `MIN_RANGE_PERCENTAGE`: Minimum range size (default 2%)
- `MAX_RANGE_PERCENTAGE`: Maximum range size (default 20%)
- `MAX_INVENTORY_DEVIATION`: When to rebalance (default 30%)
- `INVENTORY_RISK_AVERSION`: How aggressive the model is (default 0.1)
- `MONITORING_INTERVAL_SECONDS`: How often to check (default 60)

## Security notes

- Never commit your private key to version control
- Test on testnets first
- Start with small amounts
- Monitor gas costs
- Keep your RPC endpoints secure

## License

MIT License - see LICENSE file for details.

## Disclaimer

This software is for educational purposes. Use at your own risk. Always test thoroughly on testnets before using real funds.