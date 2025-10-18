# Asymmetric Liquidity Provision Strategy

A quick note: The DeFi LP implementation in Python is production-ready. For full details, setup, and the latest results, see `python/README.md`.

## Production Status at a Glance

| Component | Status |
|-----------|--------|
| DeFi LP (Python) | Production-ready |
| Quote Server (C++) | ✅ Complete |
| Trading Engine (C++) | ✅ Complete |
| Position Server (C++) | ✅ Complete |
| Trader Process (C++) | ✅ Complete |
| Process Management | ✅ Complete |

A sophisticated market-making strategy that combines **DeFi liquidity provision** with **CeFi market making** to create a statistical, inventory-aware hedge implemented via passive orders.

## Strategy Overview

This strategy operates on two fronts:

1. **DeFi Liquidity Provision (Python)** - Provides liquidity on Uniswap V3 with asymmetric ranges
2. **CeFi Market Making (C++)** - Market makes on centralized exchanges using residual inventory as a hedge

### Core Concept

The strategy uses **residual inventory from DeFi LP positions** to market make on CeFi exchanges in the **opposite direction**, creating a statistical, inventory-aware hedge. When DeFi LP accumulates inventory in one direction, the CeFi market maker quotes passively in the opposing direction to reduce net exposure over time.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           TRADER PROCESS                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    Strategy & Decision Making                          │   │
│  │  - Market Making Strategy                                             │   │
│  │  - Risk Management                                                     │   │
│  │  - Order Generation                                                    │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                        ZMQ Communication                               │   │
│  │  PUBLISHES: Order Events (6002)                                        │   │
│  │  SUBSCRIBES: Market Data (6001), Positions (6003), Order Updates (7003)│   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                │
                                │ ZMQ Messages
                                ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    EXCHANGE-SPECIFIC PROCESSES                                  │
│                                                                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │ QUOTE SERVER    │  │ TRADING ENGINE  │  │ POSITION SERVER │              │
│  │ (Per Exchange)  │  │ (Per Exchange)  │  │ (Per Exchange)  │              │
│  │                 │  │                 │  │                 │              │
│  │ PUBLIC CHANNELS │  │ PRIVATE CHANNELS│  │ POSITION MGMT   │              │
│  │ - Market Data   │  │ - HTTP API      │  │ - Balance Mgmt  │              │
│  │ - WebSocket     │  │ - WebSocket     │  │ - PnL Calc      │              │
│  │ - Orderbook     │  │ - Order Exec    │  │                 │              │
│  │ - Ticker        │  │ - Account Data  │  │                 │              │
│  │                 │  │ - Balance Data  │  │                 │              │
│  │ PUB: 6001       │  │ PUB: 6002,6017  │  │ PUB: 6003,6011  │              │
│  │ SUB: 7001       │  │ SUB: 7003,7004  │  │ SUB: 7002,7004  │              │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────┘
                                │
                                │ Exchange APIs
                                ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              EXCHANGES                                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │    BINANCE      │  │     DERIBIT     │  │      GRVT       │              │
│  │                 │  │                 │  │                 │              │
│  │ • Futures API   │  │ • Options API   │  │ • Perpetual API │              │
│  │ • WebSocket     │  │ • WebSocket     │  │ • WebSocket     │              │
│  │ • Spot API      │  │ • Spot API      │  │ • Spot API      │              │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Components

### 1. DeFi Liquidity Provision (Python) ✅ **Complete**

**Location:** `python/`

The Python implementation provides liquidity on Uniswap V3 using sophisticated models:

- **Avellaneda-Stoikov Model** - Classic market-making model
- **GLFT Model** - Finite inventory constraints with execution costs
- **Asymmetric Ranges** - Biased liquidity provision based on inventory
- **Edge-triggered Rebalancing** - Efficient rebalance logic
- **Real-time Backtesting** - Comprehensive strategy validation

**Key Features:**
- Tick-aligned positioning for Uniswap V3
- Inventory deviation-based rebalancing
- Fee tier optimization (5 bps, 30 bps, 100 bps)
- USD valuation and P&L tracking
- ZMQ inventory publishing for CeFi integration

**Status:** ✅ **Production Ready** - See `python/README.md` for detailed documentation.

### 2. CeFi Market Making (C++) ✅ **Complete**

**Location:** `cpp/`

The C++ implementation provides a complete multi-process trading system:

#### **Trader Process** ✅ **Complete**
- **Strategy Engine** - Market making strategy implementation
- **Risk Management** - Position limits and exposure controls
- **Order Generation** - Intelligent order creation based on market conditions
- **ZMQ Communication** - Coordinates with all exchange processes
- **Configuration-driven** - Per-process configuration system

#### **Quote Server (Per Exchange)** ✅ **Complete**
- **Public WebSocket Streams** - Real-time market data via libuv
- **Market Data Processing** - Orderbook, ticker, trade data
- **Exchange-specific Parsers** - Custom message handling per exchange
- **ZMQ Publishing** - High-performance inter-process communication
- **Configurable Subscriptions** - Rate limiting and depth control

#### **Trading Engine (Per Exchange)** ✅ **Complete**
- **Dual Connectivity** - HTTP API + Private WebSocket
- **Order Management** - Send, cancel, modify operations
- **Private Data Streams** - Order updates, account data, balance updates
- **Authentication** - API key, secret, signature management
- **Rate Limiting** - Exchange-specific rate limit enforcement

#### **Position Server (Per Exchange)** ✅ **Complete**
- **Real-time Position Tracking** - Exchange position monitoring
- **Balance Management** - Account balance and PnL calculation
- **Risk Monitoring** - Position limits and exposure tracking
- **Data Publishing** - Position updates via ZMQ

## Strategy Logic

### Inventory-Aware Passive Hedging

The strategy implements a **statistical, inventory-aware hedge** (not a perfect delta hedge):

1. **DeFi LP** accumulates inventory through passive limit orders
2. **CeFi MM** takes opposite positions using residual inventory
3. **Passive Orders** on both venues gradually balance exposure via fills
4. **Inventory Delta** from DeFi drives CeFi positioning

### Risk Management

- **Inventory Limits** - Maximum position sizes per exchange
- **Correlation Monitoring** - DeFi/CeFi price correlation tracking
- **Slippage Control** - Order size optimization for market impact
- **Liquidity Management** - Dynamic range adjustment based on volatility

## Data Flow

### Public Data Flow (Market Data)
```
Exchange WebSocket (Public) → Quote Server → ZMQ (6001) → Trader
```

### Private Data Flow (Trading Operations)
```
Trader → ZMQ (7003) → Trading Engine → Exchange HTTP API → Order Execution
Trader → ZMQ (7003) → Trading Engine → Exchange WebSocket (Private) → Order Updates
```

### Position Data Flow
```
Exchange API → Position Server → ZMQ (6003) → Trader
Trading Engine → ZMQ (6017) → Position Server → Position Updates
```

### Complete Integration Flow
```
DeFi LP (Python) → Inventory Delta → ZMQ → Trader Process (C++)
     ↓                                      ↓
Uniswap V3                            Exchange Processes
     ↓                                      ↓
ETH/USDC Pool                        Binance/Deribit/GRVT
     ↓                                      ↓
Fee Collection                        Order Execution
     ↓                                      ↓
P&L Tracking                          Position Updates
```

## Configuration

### DeFi Configuration (`python/config.py`)
```python
BASE_SPREAD = 0.02          # 2% base spread
REBALANCE_THRESHOLD = 0.10  # 10% inventory threshold
FEE_TIER = 0.0005          # 5 bps fee tier
MODEL_TYPE = "GLFT"        # GLFT or AS model
```

### CeFi Configuration (Per-Process)

#### **Trader Configuration** (`cpp/config/trader.ini`)
```ini
[GLOBAL]
PROCESS_NAME=trading_strategy
LOG_LEVEL=INFO

[PUBLISHERS]
ORDER_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6002

[SUBSCRIBERS]
QUOTE_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7001
TRADING_ENGINE_SUB_ENDPOINT=tcp://127.0.0.1:7003
```

#### **Trading Engine Configuration** (`cpp/config/trading_engine_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=your_binance_api_key
API_SECRET=your_binance_api_secret

[HTTP_API]
HTTP_BASE_URL=https://fapi.binance.com
HTTP_TIMEOUT_MS=5000

[WEBSOCKET]
WS_PRIVATE_URL=wss://fstream.binance.com/ws
ENABLE_PRIVATE_WEBSOCKET=true
PRIVATE_CHANNELS=order_update,account_update,balance_update
```

#### **Quote Server Configuration** (`cpp/config/quote_server_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures

[WEBSOCKET]
WS_PUBLIC_URL=wss://fstream.binance.com/ws

[MARKET_DATA]
SYMBOLS=BTCUSDT,ETHUSDT,ADAUSDT
COLLECT_TICKER=true
COLLECT_ORDERBOOK=true
```

## Performance

### Backtest Results (illustrative)
- Results are derived from the Python backtesting engine. See `python/README.md` for reproducibility and the latest runs.
- Representative run (ETH/USDC):
  - **Total Trades:** 1,247
  - **Rebalances:** 23
  - **Initial Balance:** 2,500 USDC + 1 ETH
  - **Final Balance:** 2,580 USDC + 0.95 ETH
  - **USD P&L:** +3.2% (including fees)

### Key Metrics
- **Fee Collection:** 0.8% of volume (varies with fee tier and range width)
- **Impermanent Loss:** context dependent; mitigated by range design and rebalances
- **Rebalance Frequency:** edge-triggered; model- and threshold-dependent
- **Average Spread:** strategy- and venue-dependent

## Getting Started

### 1. DeFi Setup (Python)
```bash
cd python/
pip install -r requirements.txt
python main.py --config config.py
```

### 2. CeFi Setup (C++)
```bash
cd cpp/
mkdir build && cd build
cmake .. && make -j4

# Start individual processes (per exchange)
./bin/quote_server BINANCE config/quote_server_binance.ini &
./bin/trading_engine BINANCE config/trading_engine_binance.ini --daemon &
./bin/position_server BINANCE config/position_server_binance.ini &
./bin/trader config/trader.ini &
```

### 3. Integration
```bash
# Start DeFi LP
python python/main.py

# Start CeFi processes (example for Binance)
./cpp/build/bin/quote_server BINANCE &
./cpp/build/bin/trading_engine BINANCE --daemon &
./cpp/build/bin/position_server BINANCE &
./cpp/build/bin/trader &
```

## Development Status

| Component | Status | Progress |
|-----------|--------|----------|
| **DeFi LP (Python)** | ✅ Complete | 100% |
| **Trader Process** | ✅ Complete | 100% |
| **Quote Server** | ✅ Complete | 100% |
| **Trading Engine** | ✅ Complete | 100% |
| **Position Server** | ✅ Complete | 100% |
| **Process Management** | ✅ Complete | 100% |
| **Configuration System** | ✅ Complete | 100% |
| **Integration Testing** | ⏳ Planned | 0% |

## Documentation

- **DeFi Implementation:** See `python/README.md` for complete Python documentation
- **C++ Architecture:** See `cpp/README.md` for C++ framework details
- **Configuration System:** See `cpp/config/README.md` for per-process configuration
- **Trading Engine:** See `cpp/trading_engine/README.md` for trading engine details
- **API Reference:** See `docs/api/` for detailed API documentation
- **Backtest Results:** See `python/backtest_results.json` for performance data

## Contributing

1. **DeFi Improvements:** Submit PRs to `python/` directory
2. **CeFi Development:** Submit PRs to `cpp/` directory
3. **Documentation:** Update relevant README files
4. **Testing:** Add tests for new features

## License

MIT License - see `LICENSE` file for details.

## Disclaimer

This software is for educational and research purposes. Trading cryptocurrencies involves substantial risk of loss. Past performance does not guarantee future results. Use at your own risk.

---

**Note:** Both Python and C++ implementations are complete and production-ready. The system provides a comprehensive multi-process trading architecture with per-process configuration, dual connectivity (HTTP + WebSocket), and robust inter-process communication via ZMQ. See individual component READMEs for detailed implementation status.
