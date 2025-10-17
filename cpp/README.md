# C++ Components Documentation

This document provides comprehensive documentation for the C++ components of the asymmetric LP strategy system. The C++ components handle CeFi market making with probabilistic hedging using residual inventory from DeFi LP positions.

## Architecture Overview

The system consists of four main components communicating via ZeroMQ:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   quote_server  │    │   market_maker   │    │   exec_handler   │    │ position_server │
│                 │    │                 │    │                 │    │                 │
│ • WebSocket     │───▶│ • Strategy      │───▶│ • Order          │───▶│ • Position      │
│   feeds         │    │   execution     │    │   routing        │    │   tracking      │
│ • Data          │    │ • Risk mgmt     │    │ • Exchange       │    │ • P&L calc      │
│   normalization │    │ • Hedging       │    │   integration    │    │ • Reporting     │
└─────────────────┘    └─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │                       │
         └───────────────────────┼───────────────────────┼───────────────────────┘
                                 │                       │
                    ┌─────────────▼─────────────┐       │
                    │     ZeroMQ Message Bus    │◀──────┘
                    │                           │
                    │ • Market Data (PUB/SUB)   │
                    │ • Order Events (PUB/SUB)  │
                    │ • Position Updates (PUB)  │
                    │ • Inventory Updates (SUB) │
                    └───────────────────────────┘
```

### Component Responsibilities

- **quote_server**: Connects to exchange WebSocket feeds, normalizes market data, publishes orderbooks/trades
- **market_maker**: Implements trading strategy, manages risk, sends orders, receives market data and positions
- **exec_handler**: Routes orders to exchanges, publishes execution events
- **position_server**: Tracks positions, calculates P&L, publishes position updates

### Message Flow

1. **Market Data**: `quote_server` → `market_maker` (orderbooks, trades)
2. **Orders**: `market_maker` → `exec_handler` (new orders)
3. **Order Events**: `exec_handler` → `market_maker` (fills, rejects)
4. **Positions**: `position_server` → `market_maker` (position updates)
5. **Inventory**: External → `market_maker` (DeFi LP inventory deltas)

## Prerequisites

### System Packages (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  libssl-dev \
  zlib1g-dev \
  libzmq3-dev \
  pkg-config
```

### Protocol Buffers (Optional)

For binary message serialization:
```bash
sudo apt-get install -y protobuf-compiler libprotobuf-dev
```

### External Dependencies

#### uWebSockets (Headers Only)
```bash
git clone --depth 1 https://github.com/uNetworking/uWebSockets.git /path/to/uWebSockets
# Set UWS_ROOT=/path/to/uWebSockets when building
```

#### libwebsockets (System-wide)
```bash
git clone --depth 1 https://github.com/warmcat/libwebsockets.git
cd libwebsockets && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DLWS_WITH_SHARED=ON -DLWS_WITH_ZIP_FOPS=OFF
make -j && sudo make install && sudo ldconfig
```

## Build Instructions

### Configure Build

From the repository root:

```bash
# Clean previous build
rm -rf cpp/build

# Configure with uWebSockets path
cmake -S cpp -B cpp/build -DUWS_ROOT=/absolute/path/to/uWebSockets

# Build all components
cmake --build cpp/build -j
```

### Build Outputs

- `cpp/build/quote_server/quote_server` - Market data server
- `cpp/build/market_maker` - Trading strategy
- `cpp/build/exec_handler` - Order execution handler
- `cpp/build/position_server` - Position tracking server
- `cpp/build/plugins/libexch_binance.so` - Binance exchange plugin
- `cpp/build/plugins/libexch_deribit.so` - Deribit exchange plugin

## Configuration

### Configuration File Structure

Each component uses INI files with the following structure:

```ini
# Global settings
EXCHANGES=BINANCE
SYMBOL=ethusdt
MD_PUB_ENDPOINT=tcp://127.0.0.1:6001
ORD_PUB_ENDPOINT=tcp://127.0.0.1:6002
ORD_SUB_ENDPOINT=tcp://127.0.0.1:6003
POS_PUB_ENDPOINT=tcp://127.0.0.1:6004

# Message handlers (for market_maker)
HANDLER_market_data_ENDPOINT=tcp://127.0.0.1:6001
HANDLER_market_data_TOPIC=md.BINANCE.ethusdt
HANDLER_market_data_ENABLED=true

# Exchange-specific sections
[BINANCE]
PLUGIN_PATH=/path/to/libexch_binance.so
WEBSOCKET_URL=wss://fstream.binance.com/stream
CHANNEL=depth20@100ms
CHANNEL=trade
SYMBOL=ethusdt
SYMBOL=btcusdt
```

### Key Configuration Parameters

#### Global Parameters
- `EXCHANGES`: Exchange name (BINANCE, DERIBIT)
- `SYMBOL`: Trading symbol (ethusdt, btcusdt)
- `MD_PUB_ENDPOINT`: Market data publisher endpoint
- `ORD_PUB_ENDPOINT`: Order publisher endpoint
- `ORD_SUB_ENDPOINT`: Order events subscriber endpoint
- `POS_PUB_ENDPOINT`: Position publisher endpoint

#### Quote Server Parameters
- `PUBLISH_RATE_HZ`: Market data publish rate (default: 20)
- `MAX_DEPTH`: Maximum orderbook depth (default: 50)
- `PARSER`: Parser type (BINANCE, DERIBIT)
- `SNAPSHOT_ONLY`: Use snapshot books only (default: false)
- `BOOK_DEPTH`: Snapshot depth (20, 25, 50, 100)

#### Exchange-Specific Parameters
- `PLUGIN_PATH`: Path to exchange plugin (.so file)
- `WEBSOCKET_URL`: Exchange WebSocket endpoint
- `CHANNEL`: Data channels to subscribe to
- `SYMBOL`: Symbols to subscribe to (multiple entries allowed)

#### Message Handler Parameters
- `HANDLER_<name>_ENDPOINT`: ZeroMQ endpoint
- `HANDLER_<name>_TOPIC`: ZeroMQ topic
- `HANDLER_<name>_ENABLED`: Enable/disable handler

## Running the System

### 1. Start Quote Server

```bash
./cpp/build/quote_server/quote_server -c /path/to/quote_server_config.ini
```

Example quote server config:
```ini
EXCHANGES=BINANCE
SYMBOL=ethusdt
MD_PUB_ENDPOINT=tcp://127.0.0.1:6001
PUBLISH_RATE_HZ=20
MAX_DEPTH=50
PARSER=BINANCE

[BINANCE]
PLUGIN_PATH=/home/user/asymmetric_lp/cpp/build/plugins/libexch_binance.so
WEBSOCKET_URL=wss://fstream.binance.com/stream
CHANNEL=depth20@100ms
CHANNEL=trade
SYMBOL=ethusdt
SYMBOL=btcusdt
```

### 2. Start Position Server

```bash
./cpp/build/position_server
```

### 3. Start Execution Handler

```bash
./cpp/build/exec_handler
```

### 4. Start Market Maker

```bash
./cpp/build/market_maker
```

### Complete Example Setup

Create a startup script `start_system.sh`:

```bash
#!/bin/bash

# Start components in background
./cpp/build/quote_server/quote_server -c configs/quote_server.ini &
QUOTE_PID=$!

./cpp/build/position_server &
POS_PID=$!

./cpp/build/exec_handler &
EXEC_PID=$!

sleep 2  # Allow components to start

./cpp/build/market_maker &
MM_PID=$!

echo "System started. PIDs: Quote=$QUOTE_PID, Pos=$POS_PID, Exec=$EXEC_PID, MM=$MM_PID"
echo "Press Ctrl+C to stop all components"

# Wait for interrupt
trap "kill $QUOTE_PID $POS_PID $EXEC_PID $MM_PID; exit" INT
wait
```

## ZeroMQ Message Formats

### Market Data Messages

**Topic**: `md.{EXCHANGE}.{SYMBOL}`

**Format**: Binary orderbook data
```cpp
struct OrderbookSnapshot {
  char symbol[16];
  uint64_t timestamp_us;
  uint32_t bid_count;
  uint32_t ask_count;
  struct {
    double price;
    double quantity;
  } levels[];
};
```

### Order Messages

**Topic**: `ord.new` (new orders), `ord.ev` (order events)

**Format**: Protocol Buffer or binary
```protobuf
message OrderRequest {
  string cl_ord_id = 1;
  string exch = 2;
  string symbol = 3;
  Side side = 4;
  OrderType type = 5;
  double qty = 6;
  double price = 7;
}
```

### Position Messages

**Topic**: `pos.{EXCHANGE}.{SYMBOL}`

**Format**: Binary position data
```cpp
struct PositionUpdate {
  char symbol[16];
  char exchange[8];
  double quantity;
  double avg_price;
  double unrealized_pnl;
  uint64_t timestamp_us;
};
```

## Exchange Integration

### Plugin Architecture

Exchange managers are loaded as dynamic plugins:

```cpp
// Plugin interface
extern "C" {
  ExchangeManager* create_exchange_manager(const char* websocket_url);
  void destroy_exchange_manager(ExchangeManager* mgr);
}
```

### Supported Exchanges

#### Binance Futures
- **WebSocket**: `wss://fstream.binance.com/stream`
- **Channels**: `depth20@100ms`, `trade`
- **Symbols**: `ethusdt`, `btcusdt`, etc.

#### Deribit
- **WebSocket**: `wss://www.deribit.com/ws/api/v2`
- **Channels**: `book.BTC-PERPETUAL.100ms`, `trades.BTC-PERPETUAL.100ms`
- **Symbols**: `BTC-PERPETUAL`, `ETH-PERPETUAL`

### Adding New Exchanges

1. Create exchange manager class inheriting from `IExchangeManager`
2. Implement WebSocket client for exchange API
3. Add parser for exchange-specific message formats
4. Build as MODULE library in `cpp/build/plugins/`
5. Configure plugin path in exchange section

## Development

### Code Structure

```
cpp/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # This documentation
├── quote_server/               # Market data server
│   ├── quote_server.hpp/cpp    # Main server class
│   ├── quote_server_main.cpp   # Entry point
│   ├── exchanges/              # Exchange-specific managers
│   │   ├── binance/            # Binance implementation
│   │   └── deribit/            # Deribit implementation
│   └── config.example.ini      # Example configuration
├── trader/                     # Market making strategy
│   ├── main.cpp                # Entry point
│   ├── market_making_strategy.hpp/cpp
│   └── zmq_oms.hpp/cpp         # Order management
├── utils/                      # Shared utilities
│   ├── config/                 # Configuration parsing
│   ├── handlers/               # Message handlers
│   ├── mds/                    # Market data services
│   ├── oms/                    # Order management
│   └── zmq/                    # ZeroMQ utilities
└── proto/                      # Protocol Buffer definitions
    ├── market_data.proto
    ├── order.proto
    └── position.proto
```

### Building Individual Components

```bash
# Build only quote_server
cmake --build cpp/build --target quote_server

# Build only market_maker
cmake --build cpp/build --target market_maker

# Build only plugins
cmake --build cpp/build --target exch_binance
```

### Debugging

Enable debug output by setting environment variables:

```bash
export CPP_DEBUG=1
export ZMQ_DEBUG=1
```

## Troubleshooting

### Common Issues

#### CMake Errors
- **"UWS_ROOT is not set"**: Set `-DUWS_ROOT=/path/to/uWebSockets`
- **"libwebsockets not found"**: Install libwebsockets system-wide and run `sudo ldconfig`
- **"Protobuf version mismatch"**: Align protoc and libprotobuf versions

#### Runtime Errors
- **"Config missing required keys"**: Ensure INI file has `EXCHANGES`, `SYMBOL`, `MD_PUB_ENDPOINT`
- **"Plugin not found"**: Verify `PLUGIN_PATH` points to correct `.so` file
- **"WebSocket connection failed"**: Check exchange WebSocket URL and network connectivity

#### Performance Issues
- **High CPU usage**: Reduce `PUBLISH_RATE_HZ` or `MAX_DEPTH`
- **Memory leaks**: Check for proper cleanup in destructors
- **ZMQ timeouts**: Verify endpoint addresses and ports

### Logging

Components log to stdout with timestamps:
```
[QUOTE_SERVER] Starting Quote Server Framework
[BINANCE] Connected to wss://fstream.binance.com/stream
[MARKET_MAKER] Received orderbook for ethusdt: 1000.50@1.0, 1000.75@2.0
```

### Monitoring

Monitor system health:
```bash
# Check running processes
ps aux | grep -E "(quote_server|market_maker|exec_handler|position_server)"

# Monitor ZMQ traffic
sudo netstat -tulpn | grep -E "(6001|6002|6003|6004)"

# Check plugin loading
ldd cpp/build/plugins/libexch_binance.so
```

## Performance Tuning

### Market Data Processing
- Adjust `PUBLISH_RATE_HZ` based on strategy needs
- Use `SNAPSHOT_ONLY=true` for lower latency
- Set appropriate `BOOK_DEPTH` (20-100 levels)

### Memory Management
- Monitor memory usage with `htop` or `valgrind`
- Use `MAX_DEPTH` to limit orderbook size
- Implement proper cleanup in destructors

### Network Optimization
- Use localhost endpoints for single-machine deployment
- Consider IPC endpoints for better performance
- Monitor ZMQ queue sizes

## Security Considerations

- **API Keys**: Store exchange API keys securely (not in config files)
- **Network**: Use VPN or secure networks for production
- **Permissions**: Run with minimal required privileges
- **Logging**: Avoid logging sensitive data (prices, quantities)

## Contributing

### Code Style
- Use C++17 features
- Follow RAII principles
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Prefer `const` correctness

### Testing
- Unit tests for individual components
- Integration tests for message flow
- Performance benchmarks
- Exchange connectivity tests

### Documentation
- Update this README for new features
- Document new exchange integrations
- Add configuration examples
- Include troubleshooting guides