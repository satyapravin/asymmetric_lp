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

### Live trading
```bash
python main.py
```

### Backtesting
```bash
python main.py --historical-mode --ohlc-file your_data.csv --start-date 2024-01-01 --end-date 2024-01-31
```

The backtesting mode simulates trades when price movements exceed the fee tier. It's useful for testing different parameters before going live.

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