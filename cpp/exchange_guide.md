# Exchange Integration Guide

This guide explains how to implement connectivity to a new exchange in the C++ trading system.

## Overview

Each exchange must implement 4 specialized interfaces, each used by different processes:

1. **IExchangeOMS** - Order Management System (Trading Engine Process)
2. **IExchangePMS** - Position Management System (Position Server Process)  
3. **IExchangeDataFetcher** - Startup State Recovery (Trader Process)
4. **IExchangeSubscriber** - Market Data Subscription (Market Server Process)

## Interface Requirements

### 1. IExchangeOMS (Order Management System)

**Purpose**: Order management via private WebSocket channels  
**Used by**: Trading Engine processes  
**Key Methods**:

```cpp
class IExchangeOMS {
public:
    virtual ~IExchangeOMS() = default;
    
    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Order operations
    virtual Result<std::string> place_market_order(
        const std::string& symbol,
        const std::string& side,
        double quantity) = 0;
        
    virtual Result<std::string> place_limit_order(
        const std::string& symbol,
        const std::string& side,
        double quantity,
        double price) = 0;
        
    virtual Result<bool> cancel_order(const std::string& order_id) = 0;
    virtual Result<bool> replace_order(
        const std::string& order_id,
        double new_quantity,
        double new_price) = 0;
    
    // Event handling
    virtual void set_order_status_callback(
        std::function<void(const OrderStatus&)> callback) = 0;
};
```

**Implementation Requirements**:
- Establish WebSocket connection with authentication
- Handle order placement, cancellation, and modification
- Provide real-time order status updates via callbacks
- Implement proper error handling and reconnection logic

### 2. IExchangePMS (Position Management System)

**Purpose**: Position management via private WebSocket channels  
**Used by**: Position Server processes  
**Key Methods**:

```cpp
class IExchangePMS {
public:
    virtual ~IExchangePMS() = default;
    
    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Event handling
    virtual void set_position_update_callback(
        std::function<void(const PositionUpdate&)> callback) = 0;
        
    virtual void set_account_update_callback(
        std::function<void(const AccountUpdate&)> callback) = 0;
};
```

**Implementation Requirements**:
- Establish authenticated WebSocket connection
- Subscribe to position and account update streams
- Provide real-time position and balance updates via callbacks
- Handle connection drops and reconnection

### 3. IExchangeDataFetcher (Startup State Recovery)

**Purpose**: Startup state recovery via HTTP  
**Used by**: Trader process during startup/crash recovery  
**Key Methods**:

```cpp
class IExchangeDataFetcher {
public:
    virtual ~IExchangeDataFetcher() = default;
    
    // Authentication
    virtual void set_auth_credentials(
        const std::string& api_key,
        const std::string& api_secret) = 0;
    
    // Account information
    virtual Result<AccountInfo> get_account_info() = 0;
    virtual Result<std::vector<Position>> get_positions() = 0;
    virtual Result<std::vector<Balance>> get_balances() = 0;
    
    // Order information
    virtual Result<std::vector<Order>> get_open_orders() = 0;
};
```

**Implementation Requirements**:
- Implement HTTP-based data fetching
- Handle authentication (API keys, signatures, etc.)
- Provide current account state for startup recovery
- Implement proper error handling and retries

### 4. IExchangeSubscriber (Market Data Subscription)

**Purpose**: Market data via public WebSocket channels  
**Used by**: Market Server processes  
**Key Methods**:

```cpp
class IExchangeSubscriber {
public:
    virtual ~IExchangeSubscriber() = default;
    
    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Subscription management
    virtual bool subscribe_orderbook(
        const std::string& symbol,
        int top_n = 20,
        int frequency_ms = 100) = 0;
        
    virtual bool subscribe_trades(const std::string& symbol) = 0;
    virtual bool subscribe_ticker(const std::string& symbol) = 0;
    
    // Event handling
    virtual void set_orderbook_callback(
        std::function<void(const proto::OrderBookSnapshot&)> callback) = 0;
        
    virtual void set_trade_callback(
        std::function<void(const Trade&)> callback) = 0;
        
    virtual void set_ticker_callback(
        std::function<void(const Ticker&)> callback) = 0;
};
```

**Implementation Requirements**:
- Establish public WebSocket connection (no authentication)
- Subscribe to market data streams (orderbook, trades, ticker)
- Parse exchange-specific message formats
- Provide normalized data via callbacks

## Implementation Steps

### Step 1: Create Exchange Directory Structure

```
cpp/exchanges/your_exchange/
├── oms/
│   ├── your_exchange_oms.hpp
│   └── your_exchange_oms.cpp
├── pms/
│   ├── your_exchange_pms.hpp
│   └── your_exchange_pms.cpp
├── data_fetcher/t
│   ├── your_exchange_data_fetcher.hpp
│   └── your_exchange_data_fetcher.cpp
├── subscriber/
│   ├── your_exchange_subscriber.hpp
│   └── your_exchange_subscriber.cpp
└── README.md
```

### Step 2: Implement IExchangeOMS

```cpp
// your_exchange_oms.hpp
#pragma once
#include "../../utils/oms/exchange_oms.hpp"

class YourExchangeOMS : public IExchangeOMS {
public:
    YourExchangeOMS(const std::string& api_key, const std::string& api_secret);
    ~YourExchangeOMS() override = default;
    
    // Implement all IExchangeOMS methods
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    Result<std::string> place_market_order(
        const std::string& symbol,
        const std::string& side,
        double quantity) override;
        
    // ... implement other methods
    
private:
    std::string api_key_;
    std::string api_secret_;
    std::unique_ptr<WebSocketClient> ws_client_;
    std::function<void(const OrderStatus&)> order_callback_;
    
    void handle_websocket_message(const std::string& message);
    void authenticate();
    std::string generate_signature(const std::string& data);
};
```

### Step 3: Implement IExchangePMS

```cpp
// your_exchange_pms.hpp
#pragma once
#include "../../utils/pms/exchange_pms.hpp"

class YourExchangePMS : public IExchangePMS {
public:
    YourExchangePMS(const std::string& api_key, const std::string& api_secret);
    ~YourExchangePMS() override = default;
    
    // Implement all IExchangePMS methods
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    void set_position_update_callback(
        std::function<void(const PositionUpdate&)> callback) override;
        
    // ... implement other methods
    
private:
    std::string api_key_;
    std::string api_secret_;
    std::unique_ptr<WebSocketClient> ws_client_;
    std::function<void(const PositionUpdate&)> position_callback_;
    
    void handle_websocket_message(const std::string& message);
    void subscribe_to_user_data();
};
```

### Step 4: Implement IExchangeDataFetcher

```cpp
// your_exchange_data_fetcher.hpp
#pragma once
#include "../../utils/data_fetcher/exchange_data_fetcher.hpp"

class YourExchangeDataFetcher : public IExchangeDataFetcher {
public:
    YourExchangeDataFetcher();
    ~YourExchangeDataFetcher() override = default;
    
    // Implement all IExchangeDataFetcher methods
    void set_auth_credentials(
        const std::string& api_key,
        const std::string& api_secret) override;
        
    Result<AccountInfo> get_account_info() override;
    Result<std::vector<Position>> get_positions() override;
    
    // ... implement other methods
    
private:
    std::string api_key_;
    std::string api_secret_;
    std::string base_url_;
    std::unique_ptr<HttpClient> http_client_;
    
    std::string generate_signature(const std::string& data);
    Result<std::string> make_authenticated_request(
        const std::string& endpoint,
        const std::string& method = "GET",
        const std::string& body = "");
};
```

### Step 5: Implement IExchangeSubscriber

```cpp
// your_exchange_subscriber.hpp
#pragma once
#include "../../utils/subscriber/exchange_subscriber.hpp"

class YourExchangeSubscriber : public IExchangeSubscriber {
public:
    YourExchangeSubscriber();
    ~YourExchangeSubscriber() override = default;
    
    // Implement all IExchangeSubscriber methods
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    bool subscribe_orderbook(
        const std::string& symbol,
        int top_n = 20,
        int frequency_ms = 100) override;
        
    // ... implement other methods
    
private:
    std::string base_url_;
    std::unique_ptr<WebSocketClient> ws_client_;
    std::function<void(const proto::OrderBookSnapshot&)> orderbook_callback_;
    
    void handle_websocket_message(const std::string& message);
    void parse_orderbook_message(const nlohmann::json& data);
    proto::OrderBookSnapshot normalize_orderbook(const nlohmann::json& raw_data);
};
```

### Step 6: Create Factory Classes

```cpp
// your_exchange_factory.hpp
#pragma once
#include "../../utils/factories/exchange_factory.hpp"

class YourExchangeFactory {
public:
    static std::unique_ptr<IExchangeOMS> create_oms(
        const std::string& api_key,
        const std::string& api_secret);
        
    static std::unique_ptr<IExchangePMS> create_pms(
        const std::string& api_key,
        const std::string& api_secret);
        
    static std::unique_ptr<IExchangeDataFetcher> create_data_fetcher();
    
    static std::unique_ptr<IExchangeSubscriber> create_subscriber();
};
```

### Step 7: Update Build System

Add to `cpp/exchanges/CMakeLists.txt`:

```cmake
# Your Exchange implementation
add_library(your_exchange STATIC
    your_exchange/oms/your_exchange_oms.cpp
    your_exchange/pms/your_exchange_pms.cpp
    your_exchange/data_fetcher/your_exchange_data_fetcher.cpp
    your_exchange/subscriber/your_exchange_subscriber.cpp
)

target_link_libraries(your_exchange PRIVATE utils)
```

### Step 8: Update Configuration

Add exchange-specific configuration sections:

```ini
# config/market_server_your_exchange.ini
[GLOBAL]
EXCHANGE_NAME=YOUR_EXCHANGE
ASSET_TYPE=spot

[WEBSOCKET]
WS_PUBLIC_URL=wss://your-exchange.com/ws
WS_RECONNECT_INTERVAL_MS=5000

[MARKET_DATA]
SYMBOLS=BTCUSDT,ETHUSDT
COLLECT_TICKER=true
COLLECT_ORDERBOOK=true
```

## Testing Your Implementation

### Unit Tests

Create comprehensive unit tests for each interface:

```cpp
// tests/unit/exchanges/test_your_exchange_oms.cpp
#include "doctest.h"
#include "../../exchanges/your_exchange/oms/your_exchange_oms.hpp"

TEST_SUITE("YourExchangeOMS") {
    TEST_CASE("Connection") {
        YourExchangeOMS oms("test_key", "test_secret");
        CHECK_FALSE(oms.is_connected());
        
        // Test connection logic
        bool connected = oms.connect();
        CHECK(connected == oms.is_connected());
    }
    
    TEST_CASE("Order Placement") {
        YourExchangeOMS oms("test_key", "test_secret");
        oms.connect();
        
        auto result = oms.place_limit_order("BTCUSDT", "buy", 1.0, 50000.0);
        CHECK(result.is_success());
        CHECK_FALSE(result.value().empty());
    }
    
    // Add more test cases...
}
```

### Integration Tests

Test with real exchange APIs (use testnet if available):

```cpp
TEST_CASE("Real Exchange Integration") {
    // Use testnet credentials
    YourExchangeOMS oms(testnet_api_key, testnet_api_secret);
    
    CHECK(oms.connect());
    
    // Test with small amounts
    auto result = oms.place_limit_order("BTCUSDT", "buy", 0.001, 30000.0);
    CHECK(result.is_success());
    
    // Cancel the order
    auto cancel_result = oms.cancel_order(result.value());
    CHECK(cancel_result.is_success());
}
```

## Best Practices

### Error Handling

- Use `Result<T>` pattern for all operations that can fail
- Implement proper retry logic with exponential backoff
- Handle network timeouts and connection drops gracefully
- Log all errors with sufficient context

### Authentication

- Never log API keys or secrets
- Implement proper signature generation
- Handle token refresh for OAuth-based exchanges
- Use environment variables for credentials

### Message Parsing

- Validate all incoming messages before processing
- Handle malformed JSON gracefully
- Implement proper type checking for numeric values
- Normalize data formats across exchanges

### Performance

- Use efficient data structures for orderbook management
- Implement message buffering for high-frequency updates
- Avoid unnecessary string allocations
- Use move semantics where appropriate

### Testing

- Mock external dependencies for unit tests
- Use testnet environments for integration tests
- Test error conditions and edge cases
- Implement performance benchmarks

## Common Pitfalls

1. **Authentication Issues**: Ensure proper signature generation and timestamp handling
2. **Message Format Differences**: Each exchange has unique message formats
3. **Rate Limiting**: Implement proper rate limiting to avoid API bans
4. **Connection Management**: Handle WebSocket reconnection properly
5. **Data Normalization**: Ensure consistent data formats across exchanges

## Example: Binance Implementation

See `cpp/exchanges/binance/` for a complete reference implementation of all 4 interfaces.

---

**This guide provides the foundation for implementing exchange connectivity. Follow the interface contracts carefully and implement comprehensive testing to ensure reliability.**
