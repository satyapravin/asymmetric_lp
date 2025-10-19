# Deribit Exchange Integration

This directory contains the complete Deribit exchange integration for the Asymmetric LP trading system, following the same pattern as the GRVT implementation.

## Overview

Deribit is a cryptocurrency derivatives exchange specializing in Bitcoin and Ethereum options and futures. This integration provides comprehensive connectivity for:

- **Public WebSocket**: Real-time market data (orderbook, trades, ticker, instruments)
- **Private WebSocket**: User data streams (orders, account, portfolio updates)
- **HTTP API**: RESTful data fetching and order management
- **Order Management System (OMS)**: Complete order lifecycle management

## Architecture

### Directory Structure

```
deribit/
├── public_websocket/
│   ├── deribit_public_websocket_handler.hpp
│   └── deribit_public_websocket_handler.cpp
├── private_websocket/
│   ├── deribit_private_websocket_handler.hpp
│   └── deribit_private_websocket_handler.cpp
├── http/
│   ├── deribit_data_fetcher.hpp
│   ├── deribit_data_fetcher.cpp
│   ├── deribit_oms.hpp
│   └── deribit_oms.cpp
├── deribit_manager.hpp
├── deribit_manager.cpp
└── README.md
```

### Components

#### 1. Public WebSocket Handler (`DeribitPublicWebSocketHandler`)

**Purpose**: Handles real-time market data streams from Deribit

**Key Features**:
- JSON-RPC 2.0 protocol support
- Subscription to orderbook, trades, ticker, and instrument updates
- Automatic reconnection and error handling
- Channel management (subscribe/unsubscribe)

**Supported Channels**:
- `book.{instrument_name}.{interval}` - Orderbook updates
- `trades.{instrument_name}.raw` - Trade updates
- `ticker.{instrument_name}.raw` - Ticker updates
- `instruments.{currency}` - Instrument updates

**Example Usage**:
```cpp
auto handler = std::make_unique<deribit::DeribitPublicWebSocketHandler>();
handler->connect("wss://www.deribit.com/ws/api/v2");
handler->subscribe_to_orderbook("BTC-PERPETUAL", "100ms");
handler->subscribe_to_trades("BTC-PERPETUAL");
```

#### 2. Private WebSocket Handler (`DeribitPrivateWebSocketHandler`)

**Purpose**: Handles authenticated user data streams

**Key Features**:
- OAuth 2.0 client credentials authentication
- Access token management with automatic refresh
- Private data subscriptions (orders, account, portfolio)
- Session management and reconnection

**Authentication**:
- Uses `client_id` and `client_secret` for OAuth 2.0
- Automatically obtains and refreshes access tokens
- Supports both testnet and mainnet environments

**Supported Channels**:
- `user.orders.{instrument_name}.raw` - Order updates
- `user.changes.any.any` - Account changes
- `user.portfolio.{currency}` - Portfolio updates

**Example Usage**:
```cpp
auto handler = std::make_unique<deribit::DeribitPrivateWebSocketHandler>(
    "your_client_id", "your_client_secret");
handler->connect("wss://www.deribit.com/ws/api/v2");
handler->subscribe_to_order_updates("BTC-PERPETUAL");
```

#### 3. HTTP Data Fetcher (`DeribitDataFetcher`)

**Purpose**: Fetches historical and current data via REST API

**Key Features**:
- Account information and balances
- Position and order history
- Market data (instruments, trades, orderbook, ticker, candles)
- Authentication for private endpoints

**Supported Endpoints**:
- Account summary and balances
- Open orders and order history
- Position information
- Instrument specifications
- Historical trades and candles
- Real-time orderbook and ticker

**Example Usage**:
```cpp
auto fetcher = std::make_unique<deribit::DeribitDataFetcher>(
    "your_client_id", "your_client_secret");
fetcher->connect("https://www.deribit.com/api/v2");
auto account_info = fetcher->get_account_info();
auto positions = fetcher->get_positions();
```

#### 4. Order Management System (`DeribitOMS`)

**Purpose**: Complete order lifecycle management

**Key Features**:
- Market, limit, stop, and stop-limit orders
- Advanced order types (take profit, trailing stop)
- Order modification and cancellation
- Bulk order operations
- Real-time order status tracking

**Supported Order Types**:
- **Market Orders**: Immediate execution at best available price
- **Limit Orders**: Execute at specified price or better
- **Stop Orders**: Trigger when price reaches stop level
- **Stop-Limit Orders**: Combination of stop and limit
- **Take Profit Orders**: Close position at profit target
- **Trailing Stop Orders**: Dynamic stop loss following price

**Example Usage**:
```cpp
deribit::DeribitConfig config;
config.client_id = "your_client_id";
config.client_secret = "your_client_secret";
config.testnet = true;

auto oms = std::make_unique<deribit::DeribitOMS>(config);
oms->connect();

// Place a limit order
auto result = oms->place_limit_order("BTC-PERPETUAL", "buy", 0.1, 50000.0);
if (result.is_success()) {
    std::string order_id = result.value();
    // Order placed successfully
}
```

#### 5. Exchange Manager (`DeribitManager`)

**Purpose**: Unified interface for quote server integration

**Key Features**:
- Implements `IExchangeManager` interface
- Symbol subscription management
- Message routing and formatting
- Configuration management
- Connection lifecycle management

**Example Usage**:
```cpp
auto manager = std::make_unique<deribit::DeribitManager>(
    "wss://www.deribit.com/ws/api/v2");
manager->start();
manager->subscribe_symbol("BTC-PERPETUAL");
manager->subscribe_symbol("ETH-PERPETUAL");
```

## Configuration

### Market Server Configuration

Add to `market_server/config.example.ini`:

```ini
[DERIBIT]
# Deribit public WebSocket configuration
WEBSOCKET_URL=wss://www.deribit.com/ws/api/v2
CHANNEL=book.BTC-PERPETUAL.raw
CHANNEL=trades.BTC-PERPETUAL.raw
CHANNEL=ticker.BTC-PERPETUAL.raw
SYMBOL=BTC-PERPETUAL
SYMBOL=ETH-PERPETUAL
# Testnet configuration
TESTNET=true
```

### Trading Engine Configuration

Create `trading_engine/config.deribit.ini`:

```ini
# Trading Engine Configuration - Deribit
EXCHANGE=DERIBIT
CLIENT_ID=your_deribit_client_id
CLIENT_SECRET=your_deribit_client_secret
BASE_URL=https://www.deribit.com/api/v2
TESTNET=true
CURRENCY=BTC

# WebSocket URLs
PUBLIC_WS_URL=wss://www.deribit.com/ws/api/v2
PRIVATE_WS_URL=wss://www.deribit.com/ws/api/v2

# Order management
DEFAULT_ORDER_TYPE=LIMIT
MAX_ORDER_SIZE=10.0
MIN_ORDER_SIZE=0.001

# Risk management
MAX_POSITION_SIZE=100.0
STOP_LOSS_PERCENTAGE=5.0
TAKE_PROFIT_PERCENTAGE=10.0
```

### Position Server Configuration

Add to `position_server/config.example.ini`:

```ini
[DERIBIT]
CLIENT_ID=your_deribit_client_id
CLIENT_SECRET=your_deribit_client_secret
BASE_URL=https://www.deribit.com/api/v2
SYMBOLS=BTC-PERPETUAL,ETH-PERPETUAL,BTC-28JUN24-50000-C
TESTNET=true
CURRENCY=BTC
```

## Authentication

Deribit uses OAuth 2.0 client credentials flow:

1. **Client Credentials**: `client_id` and `client_secret`
2. **Access Token**: Obtained via `/api/v2/public/auth` endpoint
3. **Token Refresh**: Automatic refresh every 30 minutes
4. **Testnet Support**: Separate testnet environment for development

### Environment Variables

```bash
export DERIBIT_CLIENT_ID="your_client_id"
export DERIBIT_CLIENT_SECRET="your_client_secret"
export DERIBIT_TESTNET="true"
```

## API Endpoints

### Public Endpoints (No Authentication)
- `GET /api/v2/public/get_instruments` - Get trading instruments
- `GET /api/v2/public/get_order_book` - Get orderbook
- `GET /api/v2/public/get_trades` - Get recent trades
- `GET /api/v2/public/ticker` - Get ticker data
- `GET /api/v2/public/get_candlestick` - Get historical candles

### Private Endpoints (Requires Authentication)
- `POST /api/v2/public/auth` - Authenticate and get access token
- `GET /api/v2/private/get_account_summary` - Get account information
- `GET /api/v2/private/get_positions` - Get current positions
- `GET /api/v2/private/get_open_orders_by_currency` - Get open orders
- `POST /api/v2/private/buy` - Place buy order
- `POST /api/v2/private/sell` - Place sell order
- `POST /api/v2/private/cancel` - Cancel order

## WebSocket Channels

### Public Channels
- `book.{instrument_name}.{interval}` - Orderbook updates
- `trades.{instrument_name}.raw` - Trade updates
- `ticker.{instrument_name}.raw` - Ticker updates
- `instruments.{currency}` - Instrument updates

### Private Channels
- `user.orders.{instrument_name}.raw` - Order updates
- `user.changes.any.any` - Account changes
- `user.portfolio.{currency}` - Portfolio updates

## Key Features

### 1. **Derivatives Focus**
- Specialized in Bitcoin and Ethereum options and futures
- Perpetual contracts with funding rates
- Options with various strike prices and expirations

### 2. **Advanced Order Types**
- Market, limit, stop, and stop-limit orders
- Take profit and trailing stop orders
- Order modification and bulk operations

### 3. **Risk Management**
- Position limits and margin requirements
- Real-time P&L calculation
- Portfolio-level risk metrics

### 4. **High Performance**
- Low-latency WebSocket connections
- Efficient JSON-RPC protocol
- Optimized for algorithmic trading

## Build System Integration

The Deribit implementation is automatically included in the exchanges shared library:

```cmake
# Deribit exchange implementation
add_library(deribit_exchange STATIC
    deribit/http/deribit_oms.hpp
    deribit/http/deribit_oms.cpp
    deribit/http/deribit_data_fetcher.hpp
    deribit/http/deribit_data_fetcher.cpp
    deribit/public_websocket/deribit_public_websocket_handler.hpp
    deribit/public_websocket/deribit_public_websocket_handler.cpp
    deribit/private_websocket/deribit_private_websocket_handler.hpp
    deribit/private_websocket/deribit_private_websocket_handler.cpp
    deribit/deribit_manager.hpp
    deribit/deribit_manager.cpp
)
```

## Testing

The implementation includes comprehensive mock functionality for testing:

- **Mock Authentication**: Simulated OAuth 2.0 flow
- **Mock Data**: Realistic test data for all endpoints
- **Mock Orders**: Simulated order management
- **Mock WebSocket**: Simulated real-time data streams

## Error Handling

Comprehensive error handling for:

- **Network Issues**: Connection failures, timeouts, reconnection
- **Authentication Errors**: Invalid credentials, token expiration
- **API Errors**: Rate limiting, invalid parameters, server errors
- **Order Errors**: Insufficient funds, invalid orders, market closed

## Performance Considerations

- **Connection Pooling**: Reuse HTTP connections
- **Message Batching**: Batch multiple subscriptions
- **Efficient Parsing**: Optimized JSON parsing
- **Memory Management**: Minimal memory allocations

## Security

- **Credential Management**: Secure storage of API keys
- **Token Security**: Automatic token refresh
- **Network Security**: TLS/SSL encryption
- **Input Validation**: Comprehensive parameter validation

## Future Enhancements

- **Advanced Order Types**: Iceberg orders, TWAP orders
- **Portfolio Analytics**: Advanced risk metrics
- **Historical Data**: Extended historical data access
- **Multi-Currency Support**: Additional cryptocurrency pairs

## Documentation

- **Deribit API Documentation**: https://docs.deribit.com/
- **WebSocket API**: https://docs.deribit.com/#websocket-api
- **REST API**: https://docs.deribit.com/#rest-api
- **Authentication**: https://docs.deribit.com/#authentication

## Support

For issues related to the Deribit integration:

1. Check the Deribit API documentation
2. Verify authentication credentials
3. Test with Deribit's testnet environment
4. Review error logs for specific error messages

The implementation follows Deribit's best practices and provides a robust foundation for algorithmic trading on the Deribit platform.
