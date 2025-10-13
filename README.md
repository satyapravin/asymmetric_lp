# Asymmetric Liquidity Provision Strategy

A quick note: The DeFi LP implementation in Python is production-ready. For full details, setup, and the latest results, see `python/README.md`.

## Production Status at a Glance

| Component | Status |
|-----------|--------|
| DeFi LP (Python) | Production-ready |
| Quote Server (C++) | In progress |
| OMS (C++) | In progress |
| Exchange Handlers (C++) | In progress |
| Position Server (C++) | In progress |
| Market Maker (C++) | In progress |

A sophisticated market-making strategy that combines **DeFi liquidity provision** with **CeFi market making** to create a statistical, inventory-aware hedge implemented via passive orders.

## Strategy Overview

This strategy operates on two fronts:

1. **DeFi Liquidity Provision (Python)** - Provides liquidity on Uniswap V3 with asymmetric ranges
2. **CeFi Market Making (C++)** - Market makes on centralized exchanges using residual inventory as a hedge

### Core Concept

The strategy uses **residual inventory from DeFi LP positions** to market make on CeFi exchanges in the **opposite direction**, creating a statistical, inventory-aware hedge. When DeFi LP accumulates inventory in one direction, the CeFi market maker quotes passively in the opposing direction to reduce net exposure over time.

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   DeFi LP       │    │   CeFi MM       │    │   Market Data   │
│   (Python)      │    │   (C++)         │    │   (C++)         │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • Uniswap V3    │    │ • Multi-exchange │    │ • Quote Server  │
│ • Asymmetric    │    │ • Order Mgmt     │    │ • Position Svr  │
│   Ranges        │    │ • Risk Mgmt      │    │ • Exec Handler  │
│ • GLFT/AS       │    │ • GLFT Model     │    │ • ZMQ IPC       │
│   Models        │    │ • Inventory      │    │ • Binary Format │
│ • Inventory     │    │   Hedging        │    │ • Real-time     │
│   Publishing    │    │ • Passive Orders│    │   Processing    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────┐
                    │   ZMQ Bus       │
                    │   (IPC Layer)   │
                    └─────────────────┘
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

### 2. CeFi Market Making (C++) 🚧 **Under Development**

**Location:** `cpp/`

The C++ implementation handles centralized exchange market making:

#### **Quote Server Framework**
- **Multi-exchange Support** - Binance, Coinbase, Kraken
- **WebSocket Integration** - Real-time market data via libuv
- **Binary Serialization** - High-performance ZMQ communication
- **Exchange-specific Parsers** - Custom message handling per exchange
- **Configurable Publishing** - Rate limiting and depth control

#### **Order Management System (OMS)**
- **Multi-exchange Routing** - Route orders to appropriate exchanges
- **Order Tracking** - Cross-exchange order status management
- **Risk Management** - Position limits and exposure controls
- **Event-driven Architecture** - Real-time order event processing

#### **Execution Handler**
- **Exchange-specific Handlers** - Binance, Coinbase implementations
- **Authentication Management** - API key, secret, passphrase handling
- **Order Lifecycle** - Send, cancel, modify operations
- **Error Handling** - Robust error recovery and retry logic

#### **Position Management System**
- **Real-time Position Tracking** - Exchange position monitoring
- **Inventory Reconciliation** - DeFi + CeFi inventory alignment
- **Hedge Calculation** - Optimal hedge ratios and timing

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

```
DeFi LP (Python) → Inventory Delta → ZMQ → CeFi MM (C++)
     ↓                                      ↓
Uniswap V3                            Exchange APIs
     ↓                                      ↓
ETH/USDC Pool                        Binance/Coinbase
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

### CeFi Configuration (`cpp/config.ini`)
```ini
EXCHANGES=BINANCE,COINBASE
SYMBOL=ETHUSDC-PERP
MIN_ORDER_QTY=0.01
MAX_ORDER_QTY=5.0
API_KEY=your_api_key
SECRET_KEY=your_secret_key
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
./quote_server/quote_server
./exch_handler/exec_handler
./trader/market_maker
```

### 3. Integration
```bash
# Start DeFi LP
python python/main.py

# Start CeFi MM
./cpp/build/trader/market_maker
```

## Development Status

| Component | Status | Progress |
|-----------|--------|----------|
| **DeFi LP (Python)** | ✅ Complete | 100% |
| **Quote Server** | 🚧 In Progress | 80% |
| **OMS Framework** | 🚧 In Progress | 70% |
| **Exchange Handlers** | 🚧 In Progress | 60% |
| **Position Management** | 🚧 In Progress | 50% |
| **Integration Testing** | ⏳ Planned | 0% |

## Documentation

- **DeFi Implementation:** See `python/README.md` for complete Python documentation
- **C++ Framework:** See `cpp/README.md` for C++ architecture details
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

**Note:** The Python implementation is complete and production-ready. The C++ implementation is under active development with the framework architecture established. See individual component READMEs for detailed implementation status.
