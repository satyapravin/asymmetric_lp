# C++ Trading System

⚠️ **Status**: Architecture complete, production testing required

A comprehensive multi-process trading system for centralized exchange market making with sophisticated order management, real-time position tracking, and strategy framework.

## Architecture Overview

The C++ system implements a clean, multi-process architecture where each exchange runs specialized processes communicating via ZeroMQ:

### Core Components

#### **Trader Process**
- **Strategy Framework**: AbstractStrategy base class with Mini OMS
- **Strategy Container**: Single strategy management with ZMQ adapters
- **Mini OMS**: Order state management and ZMQ integration
- **Risk Management**: Position limits and exposure controls

#### **Market Server (Per Exchange)**
- **Public WebSocket Streams**: Real-time market data via libwebsockets
- **Market Data Processing**: Orderbook, ticker, trade data normalization
- **Exchange-specific Parsers**: Custom message handling per exchange
- **ZMQ Publishing**: High-performance inter-process communication

#### **Trading Engine (Per Exchange)**
- **Dual Connectivity**: HTTP API + Private WebSocket
- **Order Management**: Send, cancel, modify operations
- **Private Data Streams**: Order updates, account data, balance updates
- **Authentication**: API key, secret, signature management
- **Rate Limiting**: Exchange-specific rate limit enforcement

#### **Position Server (Per Exchange)**
- **Real-time Position Tracking**: Exchange position monitoring
- **Balance Management**: Account balance and PnL calculation
- **Risk Monitoring**: Position limits and exposure tracking
- **Data Publishing**: Position updates via ZMQ

### Message Flow

1. **Market Data**: `market_server` → `trader` (orderbooks, trades)
2. **Orders**: `trader` → `trading_engine` (new orders)
3. **Order Events**: `trading_engine` → `trader` (fills, rejects)
4. **Positions**: `position_server` → `trader` (position updates)
5. **Inventory**: External → `trader` (DeFi LP inventory deltas)

### Exchange Integration

Each exchange (Binance, GRVT, Deribit) implements 4 specialized interfaces:

- **IExchangeOMS**: Order management via private WebSocket (Trading Engine)
- **IExchangePMS**: Position management via private WebSocket (Position Server)
- **IExchangeDataFetcher**: Startup state recovery via HTTP (Trader)
- **IExchangeSubscriber**: Market data via public WebSocket (Market Server)

## Configuration

### Per-Process Configuration

Each process uses INI files with exchange-specific sections:

#### **Trader Configuration** (`config/trader.ini`)
```ini
[GLOBAL]
PROCESS_NAME=trading_strategy
LOG_LEVEL=INFO
ENABLED_EXCHANGES=BINANCE,DERIBIT,GRVT

[PUBLISHERS]
ORDER_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6002
POSITION_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6003

[SUBSCRIBERS]
MARKET_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7001
TRADING_ENGINE_SUB_ENDPOINT=tcp://127.0.0.1:7003
POSITION_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7002
```

#### **Trading Engine Configuration** (`config/trading_engine_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=${BINANCE_API_KEY}
API_SECRET=${BINANCE_API_SECRET}

[HTTP_API]
HTTP_BASE_URL=https://fapi.binance.com
HTTP_TIMEOUT_MS=5000
API_REQUESTS_PER_SECOND=10

[WEBSOCKET]
WS_PRIVATE_URL=wss://fstream.binance.com/ws
ENABLE_PRIVATE_WEBSOCKET=true
PRIVATE_CHANNELS=order_update,account_update,balance_update
```

#### **Market Server Configuration** (`config/market_server_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures

[WEBSOCKET]
WS_PUBLIC_URL=wss://fstream.binance.com/ws
WS_RECONNECT_INTERVAL_MS=5000

[MARKET_DATA]
SYMBOLS=BTCUSDT,ETHUSDT,ADAUSDT
COLLECT_TICKER=true
COLLECT_ORDERBOOK=true
```

### Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `BINANCE_API_KEY` | Binance API key | Yes* |
| `BINANCE_API_SECRET` | Binance API secret | Yes* |
| `DERIBIT_CLIENT_ID` | Deribit client ID | Yes* |
| `DERIBIT_CLIENT_SECRET` | Deribit client secret | Yes* |
| `GRVT_API_KEY` | GRVT API key | Yes* |
| `GRVT_API_SECRET` | GRVT API secret | Yes* |
| `LOG_LEVEL` | Log level (DEBUG/INFO/WARN/ERROR) | No |
| `ENVIRONMENT` | Environment (development/staging/production) | No |

*At least one exchange API key pair is required

## Build System

### Prerequisites

```bash
# System packages
sudo apt-get install -y \
  build-essential \
  cmake \
  libssl-dev \
  zlib1g-dev \
  libzmq3-dev \
  pkg-config \
  protobuf-compiler \
  libprotobuf-dev

# External dependencies
git clone --depth 1 https://github.com/uNetworking/uWebSockets.git /path/to/uWebSockets
git clone --depth 1 https://github.com/warmcat/libwebsockets.git
cd libwebsockets && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DLWS_WITH_SHARED=ON
make -j && sudo make install && sudo ldconfig
```

### Build Instructions

```bash
# Clean previous build
rm -rf build

# Configure with uWebSockets path
cmake -S . -B build -DUWS_ROOT=/absolute/path/to/uWebSockets

# Build all components
cmake --build build -j

# Build specific components
cmake --build build --target market_server
cmake --build build --target trader
cmake --build build --target trading_engine
cmake --build build --target position_server
```

### Build Outputs

- `build/market_server` - Market data server
- `build/trader` - Trading strategy framework
- `build/trading_engine` - Order execution engine
- `build/position_server` - Position tracking server
- `build/plugins/libexch_binance.so` - Binance exchange plugin
- `build/plugins/libexch_deribit.so` - Deribit exchange plugin

## Testing

### Test Suite

The system includes a comprehensive test suite with **200+ test cases** across **7 categories**:

- **Integration Tests**: End-to-end workflow validation
- **Configuration System Tests**: Config management reliability
- **Utility Component Tests**: HTTP handlers, ZMQ, market data
- **Protocol Buffer Tests**: Message serialization/deserialization
- **Process-Specific Tests**: Individual process validation
- **Performance Tests**: Latency and throughput benchmarks
- **Security Tests**: Authentication and input validation

### Running Tests

```bash
# Standalone test suite (recommended)
cd tests/standalone_build/
cmake . && make run_tests
./run_tests

# Full test suite (requires all dependencies)
cd tests/
cmake . && make run_tests
./run_tests

# Test specific categories
./run_tests --test-suite="Integration Tests"
./run_tests --test-suite="Performance Tests"
```

### Test Results

- **Test Cases**: 200+ comprehensive test cases
- **Coverage**: All major system components
- **Performance**: Sub-millisecond latency benchmarks
- **Security**: Complete authentication and input validation
- **Reliability**: 100% test pass rate in standalone mode

## Running the System

### Manual Deployment

```bash
# Start Market Server
./build/market_server BINANCE config/market_server_binance.ini &

# Start Position Server
./build/position_server BINANCE config/position_server_binance.ini &

# Start Trading Engine
./build/trading_engine BINANCE config/trading_engine_binance.ini --daemon &

# Start Trader
./build/trader config/trader.ini &
```

### Docker Deployment

See `../DEPLOY.md` for complete Docker deployment instructions.

## Development Guides

### Exchange Integration

To add support for a new exchange, see **[Exchange Integration Guide](exchange_guide.md)** for:
- Implementing the 4 required interfaces (OMS, PMS, DataFetcher, Subscriber)
- WebSocket client development
- Message parsing and normalization
- Authentication and error handling

### Strategy Development

To implement a new trading strategy, see **[Strategy Development Guide](strategy_guide.md)** for:
- Inheriting from AbstractStrategy
- Implementing strategy logic
- Order management and risk controls
- Integration with Mini OMS

## Key Features

### **Clean Architecture**
- **Interface-based Design**: All exchanges implement standardized interfaces
- **Process Isolation**: Failure in one process doesn't affect others
- **Separation of Concerns**: Each process handles one specific responsibility

### **High Performance**
- **ZeroMQ Communication**: Sub-millisecond inter-process messaging
- **WebSocket Integration**: Real-time data streams
- **Multi-threading**: Concurrent processing for all components
- **Memory Optimization**: Minimal allocations and efficient data structures

### **Production Ready**
- **Comprehensive Error Handling**: Retry policies, circuit breakers, graceful degradation
- **Health Monitoring**: Process health checks and status reporting
- **Structured Logging**: JSON-formatted logs with configurable levels
- **Configuration Management**: Per-process configuration with validation

### **Extensible Design**
- **Plugin Architecture**: Exchange implementations as dynamic libraries
- **Strategy Framework**: Easy strategy development and testing
- **Protocol Buffers**: Efficient message serialization
- **Modular Components**: Independent, reusable components

## Troubleshooting

### Common Issues

#### **Build Errors**
- **"UWS_ROOT is not set"**: Set `-DUWS_ROOT=/path/to/uWebSockets`
- **"libwebsockets not found"**: Install libwebsockets system-wide and run `sudo ldconfig`
- **"Protobuf version mismatch"**: Align protoc and libprotobuf versions

#### **Runtime Errors**
- **"Config missing required keys"**: Ensure INI files have required sections
- **"Plugin not found"**: Verify plugin paths point to correct `.so` files
- **"WebSocket connection failed"**: Check exchange WebSocket URLs and network connectivity

#### **Performance Issues**
- **High CPU usage**: Reduce publish rates or message processing threads
- **Memory leaks**: Check for proper cleanup in destructors
- **ZMQ timeouts**: Verify endpoint addresses and ports

### Debug Mode

```bash
# Run specific process in debug mode
./build/trader config/trader.ini --log-level DEBUG
./build/market_server BINANCE config/market_server_binance.ini --verbose

# Enable debug output
export CPP_DEBUG=1
export ZMQ_DEBUG=1
```

## Security Considerations

- **API Key Security**: Store API keys in environment variables, never in config files
- **Network Security**: Use TLS/SSL for all connections, VPN for production
- **Process Security**: Run with minimal required privileges
- **Data Security**: Encrypt sensitive data in ZMQ messages, sanitize logs

## Performance Tuning

- **Market Data Processing**: Adjust publish rates based on strategy needs
- **Memory Management**: Monitor memory usage, implement proper cleanup
- **Network Optimization**: Use localhost endpoints for single-machine deployment
- **Resource Limits**: Set appropriate CPU and memory limits per process

---

**The C++ trading system provides a robust, scalable foundation for algorithmic trading with comprehensive exchange integration, sophisticated order management, and a flexible strategy framework. However, production testing is required before live deployment.**