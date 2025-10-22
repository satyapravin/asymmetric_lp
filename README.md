# Asymmetric Liquidity Provision Strategy

A market-making strategy that combines DeFi liquidity provision with CeFi market making to create a statistical, inventory-aware hedge implemented via passive orders.

## Strategy Overview

This strategy operates on two fronts:

1. **DeFi Liquidity Provision (Python)** - Provides liquidity on Uniswap V3 with asymmetric ranges
2. **CeFi Market Making (C++)** - Market makes on centralized exchanges using residual inventory as a hedge

### Core Concept

The strategy uses residual inventory from DeFi LP positions to market make on CeFi exchanges in the opposite direction, creating a statistical, inventory-aware hedge. When DeFi LP accumulates inventory in one direction, the CeFi market maker quotes passively in the opposing direction to reduce net exposure over time.

## Production Status

| Component | Status | Notes |
|-----------|--------|-------|
| DeFi LP (Python) | ✅ Production-ready | Complete implementation with backtesting |
| **C++ Servers** | 🔄 **UAT Pending** | Architecture complete, comprehensive test suite, sanitizer support added |
| **Trader Process** | 🔄 **UAT Pending** | Strategy framework implemented, integration testing complete |

## Quick Start

### Prerequisites

**System Dependencies (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config \
    libzmq3-dev libwebsockets-dev libssl-dev libcurl4-openssl-dev \
    libjsoncpp-dev libsimdjson-dev libprotobuf-dev protobuf-compiler \
    libuv1-dev python3 python3-pip python3-venv python3-dev
```

**For other systems, see [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)**

### Build Instructions

```bash
# Clone repository
git clone <repository-url>
cd asymmetric_lp

# Build C++ components
cd cpp
mkdir build && cd build
cmake ..
make -j$(nproc)

# Install Python dependencies
cd ../../python
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Docker Alternative

```bash
# Build and run with Docker (includes all dependencies)
docker build -t asymmetric_lp .
docker run -it asymmetric_lp
```

## Architecture

### Python DeFi Component
- **Uniswap V3 Liquidity Provision** with sophisticated models (Avellaneda-Stoikov, GLFT)
- **Asymmetric Ranges** based on inventory position
- **Real-time Backtesting** and performance validation
- **ZMQ Integration** for inventory delta publishing

### C++ CeFi Component
- **Multi-Process Architecture** with per-exchange specialization
- **Market Server** - Public WebSocket market data streams
- **Trading Engine** - Private WebSocket order execution
- **Position Server** - Real-time position and PnL tracking
- **Trader Process** - Strategy framework with Mini OMS
- **ZMQ Communication** - High-performance inter-process messaging

## Backtest Results

### Representative Performance (ETH/USDC)
- **Backtest Duration**: 3 weeks
- **Total Trades**: 1,247
- **Rebalances**: 23
- **Initial Balance**: 2,500 USDC + 1 ETH
- **Final Balance**: 2,580 USDC + 0.95 ETH
- **USD P&L**: +3.2% (including fees)

### Key Metrics
- **Fee Collection**: 0.8% of volume (varies with fee tier and range width)
- **Impermanent Loss**: Context dependent; mitigated by range design and rebalances
- **Rebalance Frequency**: Edge-triggered; model- and threshold-dependent
- **Average Spread**: Strategy- and venue-dependent

*See `python/README.md` for detailed backtesting methodology and reproducibility.*

## Documentation

- **DeFi Implementation**: See `python/README.md` for complete Python documentation
- **C++ Architecture**: See `cpp/README.md` for C++ framework details
- **Deployment & Configuration**: See `DEPLOY.md` for complete deployment guide with all configurations
- **Trading Engine**: See `cpp/trading_engine/README.md` for trading engine details
- **Test Suite**: See `cpp/tests/README.md` for comprehensive test documentation
- **Exchange Interfaces**: See `cpp/exchanges/interfaces.md` for exchange integration details

## Quick Start

### Prerequisites
- Docker and Docker Compose (recommended)
- Python 3.8+ with pip (for manual deployment)
- C++17 compiler (GCC 9+ or Clang 10+) (for manual deployment)
- CMake 3.16+ (for manual deployment)

### Docker Deployment (Recommended)
```bash
git clone <repository-url>
cd asymmetric_lp

# Create environment file
cp docker/.env.template .env
# Edit .env with your API keys and configuration

# Build and start both components
docker-compose build
docker-compose up -d

# View logs
docker-compose logs -f python-defi cpp-cefi
```

*See `DEPLOY.md` for detailed configuration and deployment instructions.*

## Contributing

1. DeFi Improvements: Submit PRs to `python/` directory
2. CeFi Development: Submit PRs to `cpp/` directory
3. Test Development: Submit PRs to `cpp/tests/` directory
4. Documentation: Update relevant README files
5. Testing: Add tests for new features using the comprehensive test framework

## License

MIT License - see `LICENSE` file for details.

## Disclaimer

This software is for educational and research purposes. Trading cryptocurrencies involves substantial risk of loss. Past performance does not guarantee future results. Use at your own risk.
