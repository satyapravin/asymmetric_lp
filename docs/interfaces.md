# Exchange Interfaces Documentation

## Architecture Overview

The exchange system is split into 4 specialized interfaces, each used by different processes:

### 1. IExchangeOMS (Trading Engine Process)
**Purpose**: Order management via private WebSocket channels
**Used by**: Trading Engine processes (one per exchange: trading_engine_binance, trading_engine_grvt, etc.)
**Key Methods**:
- `connect()` - Establishes WebSocket connection and auto-authenticates
- `place_market_order()`, `place_limit_order()`, etc. - Order placement via WebSocket
- `cancel_order()`, `replace_order()` - Order management via WebSocket
- `set_order_status_callback()` - Real-time order status updates

**Flow**: Trader → ZMQ → Trading Engine → WebSocket → Exchange

### 2. IExchangePMS (Position Server Process)
**Purpose**: Position management via private WebSocket channels
**Used by**: Position Server processes (one per exchange: position_server_binance, etc.)
**Key Methods**:
- `connect()` - Establishes WebSocket connection and auto-authenticates
- `set_position_update_callback()` - Real-time position updates
- `set_account_update_callback()` - Real-time account updates

**Flow**: Exchange → WebSocket → Position Server → ZMQ → Trader

### 3. IExchangeDataFetcher (Trader Process)
**Purpose**: Startup state recovery via HTTP
**Used by**: Trader process during startup/crash recovery
**Key Methods**:
- `get_account_info()`, `get_positions()`, `get_balances()` - Current state
- `get_open_orders()` - Current open orders
- `set_auth_credentials()` - HTTP authentication

**Flow**: Trader → HTTP → Exchange (one-time during startup)

### 4. IExchangeSubscriber (Market Server Process)
**Purpose**: Market data via public WebSocket channels
**Used by**: Market Server processes (one per exchange: market_server_binance, etc.)
**Key Methods**:
- `connect()` - Establishes WebSocket connection
- `subscribe_orderbook(symbol, top_n, frequency_ms)` - Configurable orderbook snapshots
- `subscribe_trades(symbol)` - Trade data
- `set_orderbook_callback()`, `set_trade_callback()` - Real-time market data

**Flow**: Exchange → WebSocket → Market Server → ZMQ → Trader

## Key Design Principles

1. **Separation of Concerns**: Each interface handles one specific responsibility
2. **Process Isolation**: Each interface is used by a separate process
3. **WebSocket for Live Data**: Private channels for orders/positions, public for market data
4. **HTTP for Recovery**: Only used during startup to get current state
5. **Real-time Callbacks**: Live updates via WebSocket callbacks, not polling
6. **Exchange Abstraction**: Each exchange implements all 4 interfaces consistently
7. **No Shared State**: Each process manages its own WebSocket connections
8. **Centralized Logging**: All exchange implementations use logging macros (LOG_DEBUG_COMP, LOG_INFO_COMP, LOG_ERROR_COMP, LOG_WARN_COMP) instead of std::cout/std::cerr

## Exchange Implementations

Each exchange (Binance, GRVT, Deribit) must implement all 4 interfaces:
- `BinanceOMS`, `BinancePMS`, `BinanceDataFetcher`, `BinanceSubscriber`
- `GrvtOMS`, `GrvtPMS`, `GrvtDataFetcher`, `GrvtSubscriber`
- `DeribitOMS`, `DeribitPMS`, `DeribitDataFetcher`, `DeribitSubscriber`

## Configuration

- **No separate config files** for exchange interfaces
- **Configuration handled by processes**: Market Server configures orderbook frequency/top_n
- **Authentication**: Each interface manages its own credentials
- **Error Handling**: All interfaces use `Result<T>` for consistent error handling
