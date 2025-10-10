# OHLC Data Downloader

This module provides functionality to download 1-second OHLC (Open, High, Low, Close) bars from Uniswap V3 pools on various blockchain networks.

## Features

- **Multi-chain support**: Ethereum, Polygon, Arbitrum, Optimism, Base
- **Flexible timeframes**: Download data for any time range
- **Configurable intervals**: 1-second, 1-minute, or custom intervals
- **Real-time data**: Uses actual swap events from the blockchain
- **CSV export**: Saves data in the format expected by the backtest engine

## Quick Start

### 1. Install Dependencies

All required dependencies are already in `requirements.txt`:
```bash
pip install -r requirements.txt
```

### 2. Configure Environment

Set up your `.env` file with chain configuration:

```bash
# Copy from chain.conf and customize
ETHEREUM_RPC_URL=https://mainnet.infura.io/v3/YOUR_PROJECT_ID
CHAIN_ID=1
CHAIN_NAME=Ethereum Mainnet
# ... other chain-specific settings
```

### 3. Download OHLC Data

#### Command Line Usage

```bash
# Download 1-second ETH/USDC bars for last hour
python ohlc_downloader.py \
  --token0 0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2 \
  --token1 0xA0b86a33E6441b8c4C8C0E1234567890abcdef123 \
  --fee 3000 \
  --start-time "2024-01-15 10:00:00" \
  --end-time "2024-01-15 11:00:00" \
  --interval 1 \
  --output eth_usdc_1sec.csv
```

#### Python API Usage

```python
from datetime import datetime, timedelta, timezone
from ohlc_downloader import OHLCDataDownloader
from config import Config

# Initialize
config = Config()
downloader = OHLCDataDownloader(config)

# Define parameters
WETH = "0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2"
USDC = "0xA0b86a33E6441b8c4C8C0E1234567890abcdef123"

end_time = datetime.now(timezone.utc)
start_time = end_time - timedelta(hours=1)

# Download data
ohlc_bars = downloader.download_ohlc_data(
    token0=WETH,
    token1=USDC,
    fee=3000,  # 0.3% fee tier
    start_time=start_time,
    end_time=end_time,
    interval_seconds=1
)

# Save to CSV
downloader.save_ohlc_data(ohlc_bars, "eth_usdc_1sec.csv")
```

## Token Addresses

### Ethereum Mainnet
- **WETH**: `0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2`
- **USDC**: `0xA0b86a33E6441b8c4C8C0E1234567890abcdef123` (example)
- **USDT**: `0xdAC17F958D2ee523a2206206994597C13D831ec7`
- **WBTC**: `0x2260FAC5E5542a773Aa44fBCfeDf7C193bc2C599`
- **DAI**: `0x6B175474E89094C44Da98b954EedeAC495271d0F`

### Fee Tiers
- **500**: 0.05% (5 bps) - Stable pairs
- **3000**: 0.3% (30 bps) - Standard pairs
- **10000**: 1% (100 bps) - Exotic pairs

## Data Format

The downloaded CSV files follow the exact format expected by the backtest engine:

```csv
timestamp,open,high,low,close,volume
2024-01-15 10:00:00,2500.12345678,2501.23456789,2499.87654321,2500.98765432,150.50
2024-01-15 10:00:01,2500.98765432,2502.34567890,2500.12345678,2501.87654321,200.30
```

## Examples

### Run Example Scripts

```bash
# Run the example script
python download_example.py
```

This will download:
- ETH/USDC 1-minute bars for the last hour
- BTC/USDC 1-second bars for the last 10 minutes

### Common Use Cases

#### 1. Recent High-Frequency Data
```bash
# Last 30 minutes of 1-second data
python ohlc_downloader.py \
  --token0 0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2 \
  --token1 0xA0b86a33E6441b8c4C8C0E1234567890abcdef123 \
  --fee 3000 \
  --start-time "$(date -d '30 minutes ago' '+%Y-%m-%d %H:%M:%S')" \
  --end-time "$(date '+%Y-%m-%d %H:%M:%S')" \
  --interval 1 \
  --output recent_eth_usdc.csv
```

#### 2. Historical Data for Backtesting
```bash
# Full day of 1-minute data
python ohlc_downloader.py \
  --token0 0x2260FAC5E5542a773Aa44fBCfeDf7C193bc2C599 \
  --token1 0xA0b86a33E6441b8c4C8C0E1234567890abcdef123 \
  --fee 3000 \
  --start-time "2024-01-01 00:00:00" \
  --end-time "2024-01-01 23:59:59" \
  --interval 60 \
  --output btc_usdc_20240101.csv
```

## Technical Details

### How It Works

1. **Pool Discovery**: Finds the Uniswap V3 pool for the specified token pair and fee tier
2. **Block Range Estimation**: Estimates the block range for the requested time period
3. **Event Fetching**: Retrieves all Swap events from the pool within the block range
4. **OHLC Construction**: Groups swap events by time intervals and constructs OHLC bars
5. **Price Conversion**: Converts sqrt price X96 values to human-readable prices
6. **Volume Calculation**: Aggregates trading volumes from swap amounts

### Performance Considerations

- **RPC Limits**: Be mindful of your RPC provider's rate limits
- **Block Range**: Large time ranges may require significant RPC calls
- **Memory Usage**: High-frequency data can consume substantial memory
- **Network Latency**: Blockchain queries have inherent latency

### Error Handling

The downloader includes robust error handling for:
- Invalid token addresses
- Non-existent pools
- RPC connection issues
- Block range estimation errors
- Price conversion failures

## Integration with Backtest Engine

The downloaded CSV files can be directly used with the backtest engine:

```bash
# Use downloaded data for backtesting
python main.py --historical-mode \
  --ohlc-file eth_usdc_1sec.csv \
  --start-date 2024-01-15 \
  --end-date 2024-01-15
```

## Troubleshooting

### Common Issues

1. **"No pool found"**: Check token addresses and fee tier
2. **"RPC connection failed"**: Verify ETHEREUM_RPC_URL in .env
3. **"No swap events found"**: Try a different time range or longer interval
4. **"Block range too large"**: Reduce the time range or use a faster RPC provider

### Performance Tips

- Use shorter time ranges for 1-second data
- Consider 1-minute intervals for historical data
- Use premium RPC providers for better performance
- Cache token decimals to reduce RPC calls

## Support

For issues or questions:
1. Check the logs for detailed error messages
2. Verify your configuration in `.env`
3. Test with the example script first
4. Ensure your RPC provider supports the required block range
