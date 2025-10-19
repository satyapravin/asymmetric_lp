# Exchange Configuration Guide

## Overview
Each exchange has its own configuration file located in its respective folder. This provides better organization and maintainability.

## Configuration Structure

### Master Configuration
The main configuration file is `config/master_config.json` which references individual exchange configs:

```json
{
  "exchanges": {
    "binance": {
      "config_file": "binance/binance_config.json",
      "enabled": true
    },
    "grvt": {
      "config_file": "grvt/grvt_config.json", 
      "enabled": true
    },
    "deribit": {
      "config_file": "deribit/deribit_config.json",
      "enabled": true
    }
  },
  "global_settings": {
    "default_timeout_ms": 5000,
    "max_retries": 3,
    "log_level": "INFO",
    "enable_testnet": false
  }
}
```

### Exchange-Specific Configurations

#### Binance: `binance/binance_config.json`
- Supports FUTURES and SPOT assets
- Uses API key + signature authentication
- WebSocket channels: `depth`, `trade`, `ticker`, `userDataStream`

#### GRVT: `grvt/grvt_config.json`
- Supports PERPETUAL assets
- Uses API key + session cookie authentication
- WebSocket channels: `orderbook`, `trades`, `ticker`, `user`

#### Deribit: `deribit/deribit_config.json`
- Supports PERPETUAL assets
- Uses OAuth 2.0 client credentials
- WebSocket channels: `book`, `trades`, `ticker`, `user`

## Key Features

### 1. **Flexible URL Configuration**
Each asset type can have multiple URL types:
- `rest_api`: HTTP REST API base URL
- `websocket_public`: Public WebSocket URL
- `websocket_private`: Private WebSocket URL
- `websocket_market_data`: Market data WebSocket URL
- `websocket_user_data`: User data WebSocket URL

### 2. **Protocol-Specific Usage**

#### For HTTP REST API (Data Fetcher - READ ONLY):
```cpp
// Get REST API URL for Binance Futures
std::string rest_url = config_manager.get_rest_api_url("BINANCE", AssetType::FUTURES);
// Returns: "https://fapi.binance.com"

// HTTP REST API is ONLY used for:
// - get_open_orders()     - Get current open orders
// - get_positions()        - Get current positions  
// - get_account()          - Get account information
// - get_order_history()    - Get historical orders
```

#### For WebSocket (OMS, PMS, Subscriber - READ/WRITE):
```cpp
// Get public WebSocket URL for market data
std::string ws_url = config_manager.get_websocket_url("BINANCE", AssetType::FUTURES, "public");
// Returns: "wss://fstream.binance.com/stream"

// Get private WebSocket URL for user data (orders, positions)
std::string private_ws_url = config_manager.get_websocket_url("BINANCE", AssetType::FUTURES, "private");
// Returns: "wss://fstream.binance.com/ws"

// WebSocket is used for:
// - place_order()          - Place new orders (real-time)
// - cancel_order()          - Cancel orders (real-time)
// - modify_order()          - Modify orders (real-time)
// - position_updates        - Real-time position updates
// - order_updates          - Real-time order status updates
// - market_data            - Real-time market data
```

### 3. **Private WebSocket Order Management**

#### How Order Placement Works:
```cpp
// 1. Connect to private WebSocket
std::string private_ws_url = config_manager.get_websocket_url("BINANCE", AssetType::FUTURES, "private");
// Returns: "wss://fstream.binance.com/ws"

// 2. Get order channel name
std::string order_channel = config_manager.get_websocket_channel_name("BINANCE", AssetType::FUTURES, "place_order");
// Returns: "order"

// 3. Send order via WebSocket (not HTTP!)
websocket.send({
    "method": "order.place",
    "params": {
        "symbol": "BTCUSDT",
        "side": "BUY",
        "type": "LIMIT",
        "quantity": "0.1",
        "price": "50000"
    }
});

// 4. Listen for order updates on order_updates channel
std::string updates_channel = config_manager.get_websocket_channel_name("BINANCE", AssetType::FUTURES, "order_updates");
// Returns: "orderUpdate"
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

## Benefits of New Structure

1. **Modular Design**: Each exchange has its own config file
2. **Clear Separation**: HTTP vs WebSocket URLs are clearly separated
3. **Protocol Awareness**: Each URL type is explicitly defined
4. **Exchange Flexibility**: Each exchange can have different URL patterns
5. **Channel Mapping**: WebSocket channel names are exchange-specific
6. **Authentication**: Auth methods are clearly defined per exchange
7. **Testnet Support**: Easy to switch between mainnet/testnet URLs
8. **Maintainability**: Easy to update individual exchange configs