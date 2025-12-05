# Exchange Configuration Guide

## Overview
The system uses **INI files** for process configuration and **JSON files** for exchange-specific API configurations. This hybrid approach provides flexibility and maintainability.

## Configuration Structure

### Process Configuration (INI Files)
Each process uses INI files for runtime configuration:

**Test Configuration** (`cpp/tests/test_config.ini`):
```ini
[market_server]
exchange = binance
symbol = BTCUSDT
zmq_publisher_address = tcp://127.0.0.1:5555

[trading_engine]
exchange_config = ../tests/config/binance_oms.json

[zmq]
mds_subscribe_endpoint = tcp://127.0.0.1:5555
mds_topic = market_data
pms_subscribe_endpoint = tcp://127.0.0.1:5556
pms_topic = position_updates
oms_publish_endpoint = tcp://127.0.0.1:5557
oms_subscribe_endpoint = tcp://127.0.0.1:5558
oms_event_topic = order_events

[position_server]
exchange = binance
```

### Exchange API Configuration (JSON Files)
Exchange-specific API configurations are stored in JSON format:

**Individual Exchange Configurations**:
- `cpp/exchanges/binance/binance_config.json` - Binance API configuration
- `cpp/exchanges/grvt/grvt_config.json` - GRVT API configuration  
- `cpp/exchanges/deribit/deribit_config.json` - Deribit API configuration

### Exchange-Specific Configurations

#### Binance: `cpp/exchanges/binance/binance_config.json`
- Supports FUTURES and SPOT assets
- Uses API key + signature authentication
- WebSocket channels: `depth`, `trade`, `ticker`, `userDataStream`
- Testnet URLs: `dstream.binancefuture.com` (testnet), `fstream.binance.com` (mainnet)

#### GRVT: `cpp/exchanges/grvt/grvt_config.json`
- Supports PERPETUAL assets
- Uses API key + session cookie authentication
- WebSocket channels: `orderbook`, `trades`, `ticker`, `user`

#### Deribit: `cpp/exchanges/deribit/deribit_config.json`
- Supports PERPETUAL assets
- Uses OAuth 2.0 client credentials
- WebSocket channels: `book`, `trades`, `ticker`, `user`

## Configuration Usage

### Process Configuration Manager
The system uses `ProcessConfigManager` for INI file handling:

```cpp
#include "utils/config/process_config_manager.hpp"

// Load configuration
ProcessConfigManager config;
config.load_config("test_config.ini");

// Get values
std::string exchange = config.get_string("[market_server]", "exchange");
std::string symbol = config.get_string("[market_server]", "symbol");
std::string zmq_endpoint = config.get_string("[zmq]", "mds_subscribe_endpoint");
```

### Exchange Configuration Usage
Individual exchange JSON files are loaded directly by exchange implementations:

```cpp
// Exchange implementations load their own config files directly
// Example: BinanceOMS loads binance_config.json
// Example: DeribitOMS loads deribit_config.json
```

## Key Features

### 1. **Hybrid Configuration System**
- **INI Files**: Process runtime configuration (ZMQ endpoints, exchange names, etc.)
- **JSON Files**: Exchange API configuration (URLs, endpoints, authentication)

### 2. **Flexible URL Configuration**
Each asset type can have multiple URL types:
- `rest_api`: HTTP REST API base URL
- `websocket_public`: Public WebSocket URL
- `websocket_private`: Private WebSocket URL
- `websocket_market_data`: Market data WebSocket URL
- `websocket_user_data`: User data WebSocket URL

### 3. **Protocol-Specific Usage**

#### For HTTP REST API (Data Fetcher - READ ONLY):
```cpp
// Exchange implementations use their own config loading
// Example: BinanceOMS loads binance_config.json directly
// Returns: "https://testnet.binancefuture.com"

// HTTP REST API is ONLY used for:
// - get_open_orders()     - Get current open orders
// - get_positions()        - Get current positions  
// - get_account()          - Get account information
```

#### For WebSocket (OMS, PMS, Subscriber - READ/WRITE):
```cpp
// Exchange implementations use their own config loading
// Example: BinanceOMS loads binance_config.json directly
// Returns: "wss://dstream.binancefuture.com/stream"

// WebSocket is used for:
// - place_order()          - Place new orders (real-time)
// - cancel_order()          - Cancel orders (real-time)
// - modify_order()          - Modify orders (real-time)
// - position_updates        - Real-time position updates
// - order_updates          - Real-time order status updates
// - market_data            - Real-time market data
```

#### Exchange-Specific Order Channels:

**Binance:**
- `place_order`: "order" 
- `cancel_order`: "order"
- `order_updates`: "orderUpdate"
- `position_updates`: "accountUpdate"

**GRVT:**
- `place_order`: "order"
- `cancel_order`: "order" 
- `order_updates`: "orderUpdate"
- `position_updates`: "positionUpdate"

**Deribit:**
- `place_order`: "buy"
- `cancel_order`: "cancel"
- `order_updates`: "user.orders"
- `position_updates`: "user.portfolio"

## Benefits of Current Structure

1. **Hybrid Design**: INI for process config, JSON for API config
2. **Clear Separation**: Process runtime vs exchange API configuration
3. **Protocol Awareness**: Each URL type is explicitly defined
4. **Exchange Flexibility**: Each exchange can have different URL patterns
5. **Channel Mapping**: WebSocket channel names are exchange-specific
6. **Authentication**: Auth methods are clearly defined per exchange
7. **Testnet Support**: Easy to switch between mainnet/testnet URLs
8. **Maintainability**: Easy to update individual exchange configs
9. **Process Isolation**: Each process has its own INI configuration
10. **ZMQ Integration**: ZMQ endpoints clearly defined in INI files