# Exchange WebSocket Handler Tests

This directory contains comprehensive test suites for exchange-specific WebSocket handlers, organized by connectivity type and exchange.

## Test Structure

```
tests/unit/exchanges/
├── binance/
│   ├── public_websocket/     # Market data stream tests
│   │   └── test_binance_public_websocket_handler.cpp
│   ├── private_websocket/    # User data stream tests
│   │   └── test_binance_private_websocket_handler.cpp
│   └── http/                # HTTP API tests
│       ├── test_binance_data_fetcher.cpp
│       └── test_binance_oms.cpp
├── deribit/                 # Future: Deribit tests
└── grvt/                    # Future: GRVT tests
```

## Test Categories

### Public WebSocket Handler Tests
- **Connection Management**: Connect, disconnect, reconnection
- **Subscription Management**: Orderbook, ticker, trades, klines
- **Message Handling**: Callback execution, data parsing
- **Error Handling**: Invalid inputs, connection failures
- **Thread Safety**: Concurrent operations, callback safety

### Private WebSocket Handler Tests
- **Authentication**: API key handling, listen key management
- **User Data Streams**: Account updates, order updates, balance updates
- **Subscription Management**: User data, order updates, account updates
- **Message Handling**: Private data parsing, callback execution
- **Error Handling**: Authentication failures, invalid credentials
- **Thread Safety**: Concurrent operations, callback safety

### HTTP Handler Tests (Data Fetcher)
- **Data Retrieval**: Account info, positions, orders, balances
- **Connection Management**: HTTP connection handling
- **Error Handling**: Invalid symbols, network failures
- **Rate Limiting**: API rate limit compliance
- **Thread Safety**: Concurrent requests, callback safety

### HTTP Handler Tests (OMS)
- **Order Management**: Place, cancel, modify orders
- **Order Types**: Market, limit, stop, stop-limit orders
- **Order Lifecycle**: Complete order workflow testing
- **Data Retrieval**: Order status, order history, account info
- **Error Handling**: Invalid parameters, order failures
- **Thread Safety**: Concurrent operations, callback safety

## Running Tests

```bash
# Build tests
cd cpp/build
make run_tests

# Run tests
./tests/run_tests

# Run specific test suite
./tests/run_tests --test-suite="BinancePublicWebSocketHandler"
./tests/run_tests --test-suite="BinancePrivateWebSocketHandler"
./tests/run_tests --test-suite="BinanceDataFetcher"
./tests/run_tests --test-suite="BinanceOMS"
```

## Test Coverage

Each test suite covers:

1. **Basic Functionality**
   - Constructor/destructor
   - Initialize/shutdown
   - Connect/disconnect

2. **Core Operations**
   - Subscription management
   - Message sending/receiving
   - Data retrieval/processing

3. **Error Handling**
   - Invalid inputs
   - Connection failures
   - Network errors

4. **Thread Safety**
   - Concurrent operations
   - Callback thread safety
   - Data race prevention

5. **Integration**
   - End-to-end workflows
   - Real-world scenarios
   - Performance testing

## Mock vs Real Testing

- **Unit Tests**: Use mock implementations for isolated testing
- **Integration Tests**: Use real API endpoints with test credentials
- **Performance Tests**: Measure latency, throughput, memory usage

## Test Data

- **Test Symbols**: BTCUSDT, ETHUSDT, ADAUSDT, DOTUSDT
- **Test Credentials**: Use environment variables for API keys
- **Test Scenarios**: Cover typical trading operations

## Future Enhancements

- Add Deribit and GRVT test suites
- Implement integration tests with real exchanges
- Add performance benchmarking
- Add stress testing for high-frequency scenarios
- Add mock exchange servers for offline testing
