# Trading Engine Documentation

⚠️ **Status**: Architecture complete, production testing required

## Overview

The Trading Engine is a critical component of the multi-process trading system, responsible for executing orders and managing private data streams for each exchange. It provides dual connectivity through HTTP API and WebSocket for comprehensive trading operations.

## Architecture

### Dual Connectivity Model

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           TRADING ENGINE                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    HTTP API Handler                                   │   │
│  │  - Order Placement                                                    │   │
│  │  - Order Cancellation                                                 │   │
│  │  - Order Modification                                                 │   │
│  │  - Account Queries                                                     │   │
│  │  - Authentication & Signatures                                        │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    WebSocket Manager                                   │   │
│  │  - Order Updates                                                       │   │
│  │  - Account Updates                                                     │   │
│  │  - Balance Updates                                                     │   │
│  │  - Trade Executions                                                    │   │
│  │  - Real-time Data Streams                                              │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                        ZMQ Communication                              │   │
│  │  PUBLISHES: Order Events (6002), Trade Events (6017), Order Status (6018)│   │
│  │  SUBSCRIBES: Trader Orders (7003), Position Updates (7004)            │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                │
                                │ Exchange APIs
                                ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              EXCHANGE                                           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │   HTTP API      │  │  WebSocket      │  │   Authentication│              │
│  │                 │  │                 │  │                 │              │
│  │ • Order Endpoints│  │ • Private Streams│  │ • API Keys      │              │
│  │ • Account Info  │  │ • User Data     │  │ • Signatures     │              │
│  │ • Position Data │  │ • Real-time     │  │ • Rate Limiting  │              │
│  │ • Trade History │  │ • Updates       │  │ • Timestamps     │              │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Key Features

### ✅ **HTTP API Operations**
- **Order Management**: Send, cancel, modify orders via REST API
- **Account Queries**: Real-time account information and balances
- **Position Tracking**: Current positions and PnL calculations
- **Authentication**: Secure API key and signature management
- **Rate Limiting**: Exchange-specific rate limit enforcement

### ✅ **WebSocket Operations**
- **Private Data Streams**: Real-time order updates and account changes
- **User Data Streams**: Balance updates and position changes
- **Trade Executions**: Immediate trade execution notifications
- **Connection Management**: Automatic reconnection and error handling

### ✅ **ZMQ Communication**
- **Order Events**: Publish order status updates to trader
- **Trade Events**: Publish trade executions to position server
- **Order Status**: Real-time order state changes
- **Inter-Process Messaging**: High-performance communication

## Configuration

### Trading Engine Configuration File

```ini
# =============================================================================
# GLOBAL CONFIGURATION
# =============================================================================
[GLOBAL]
PROCESS_NAME=trading_engine_binance
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=your_binance_api_key
API_SECRET=your_binance_api_secret
TESTNET_MODE=false
LOG_LEVEL=INFO

# =============================================================================
# ZMQ COMMUNICATION
# =============================================================================
[ZMQ]
ORDER_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6002
TRADE_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6017
ORDER_STATUS_PUB_ENDPOINT=tcp://127.0.0.1:6018
TRADER_SUB_ENDPOINT=tcp://127.0.0.1:7003
POSITION_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7004

# =============================================================================
# ORDER MANAGEMENT
# =============================================================================
[ORDER_MANAGEMENT]
MAX_ORDERS_PER_SECOND=10
ORDER_TIMEOUT_MS=5000
RETRY_FAILED_ORDERS=true
MAX_ORDER_RETRIES=3
ORDER_QUEUE_SIZE=1000
ORDER_PROCESSING_THREADS=2

# =============================================================================
# HTTP API CONFIGURATION
# =============================================================================
[HTTP_API]
HTTP_BASE_URL=https://fapi.binance.com
HTTP_TIMEOUT_MS=5000
HTTP_MAX_RETRIES=3
HTTP_RETRY_DELAY_MS=1000
API_REQUESTS_PER_SECOND=10
API_REQUESTS_PER_MINUTE=1200
API_BURST_LIMIT=20
AUTH_METHOD=HMAC_SHA256
SIGNATURE_TIMESTAMP_TOLERANCE_MS=5000
REFRESH_CREDENTIALS_INTERVAL_MS=3600000

# =============================================================================
# WEBSOCKET CONFIGURATION (Private Channels Only)
# =============================================================================
[WEBSOCKET]
WS_PRIVATE_URL=wss://fstream.binance.com/ws
WS_PRIVATE_BACKUP_URL=wss://fstream.binance.com/ws
WS_RECONNECT_INTERVAL_MS=5000
WS_PING_INTERVAL_MS=30000
WS_PONG_TIMEOUT_MS=10000
WS_MAX_RECONNECT_ATTEMPTS=10
WS_CONNECTION_TIMEOUT_MS=10000
WS_MAX_MESSAGE_SIZE_KB=1024
WS_MESSAGE_BUFFER_SIZE=10000
WS_PROCESSING_THREADS=2
ENABLE_PRIVATE_WEBSOCKET=true
PRIVATE_CHANNELS=order_update,account_update,balance_update,trade_update

# =============================================================================
# RISK MANAGEMENT
# =============================================================================
[RISK_MANAGEMENT]
MAX_POSITION_SIZE=10.0
MAX_POSITION_PER_SYMBOL=5.0
MAX_DAILY_LOSS=5000.0
ENABLE_PRE_TRADE_CHECKS=true
POSITION_LIMIT_CHECK_INTERVAL_MS=1000
RISK_ALERT_THRESHOLD=0.8

# =============================================================================
# PERFORMANCE
# =============================================================================
[PERFORMANCE]
MAX_WORKER_THREADS=4
MESSAGE_PROCESSING_THREADS=2
HTTP_POOL_SIZE=10
WEBSOCKET_POOL_SIZE=5
MEMORY_LIMIT_MB=512
CPU_LIMIT_PERCENT=80

# =============================================================================
# MONITORING
# =============================================================================
[MONITORING]
ENABLE_METRICS=true
METRICS_INTERVAL_MS=1000
HEALTH_CHECK_INTERVAL_MS=5000
LOG_PERFORMANCE_METRICS=true
ALERT_ON_ERRORS=true
```

## API Operations

### HTTP API Methods

#### Order Operations
```cpp
// Send order via HTTP
bool send_order_via_http(const OrderRequest& request);

// Cancel order via HTTP
bool cancel_order_via_http(const std::string& cl_ord_id);

// Modify order via HTTP
bool modify_order_via_http(const std::string& cl_ord_id, double new_price, double new_qty);

// Query order status via HTTP
bool query_order_via_http(const std::string& cl_ord_id);
```

#### Account Operations
```cpp
// Query account information
bool query_account_via_http();

// Get current positions
bool query_positions_via_http();

// Get account balances
bool query_balances_via_http();
```

### WebSocket Operations

#### Connection Management
```cpp
// Connect to private WebSocket
bool connect_private_websocket();

// Disconnect from WebSocket
void disconnect_private_websocket();

// Subscribe to private channels
bool subscribe_to_private_channels();

// Check connection status
bool is_private_websocket_connected() const;
```

#### Event Handling
```cpp
// Handle order updates
void handle_order_update(const std::string& order_id, const std::string& status);

// Handle trade executions
void handle_trade_update(const std::string& trade_id, double qty, double price);

// Handle account updates
void handle_account_update(const std::string& account_data);

// Handle balance updates
void handle_balance_update(const std::string& balance_data);
```

## Data Flow

### Order Execution Flow
```
1. Trader → ZMQ (7003) → Trading Engine
2. Trading Engine → HTTP API → Exchange
3. Exchange → WebSocket → Trading Engine
4. Trading Engine → ZMQ (6002) → Trader
```

### Account Update Flow
```
1. Exchange → WebSocket → Trading Engine
2. Trading Engine → ZMQ (6017) → Position Server
3. Position Server → ZMQ (6003) → Trader
```

### Trade Execution Flow
```
1. Exchange → WebSocket → Trading Engine
2. Trading Engine → ZMQ (6017) → Position Server
3. Position Server → ZMQ (6003) → Trader
```

## Error Handling

### HTTP API Errors
- **Authentication Errors**: Invalid API keys or signatures
- **Rate Limit Errors**: Too many requests per second/minute
- **Network Errors**: Connection timeouts and retries
- **Order Errors**: Invalid order parameters or insufficient funds

### WebSocket Errors
- **Connection Errors**: Network connectivity issues
- **Authentication Errors**: Invalid WebSocket credentials
- **Message Errors**: Malformed or invalid messages
- **Reconnection Logic**: Automatic reconnection with exponential backoff

### Error Recovery
- **Retry Policies**: Configurable retry attempts with backoff
- **Circuit Breakers**: Automatic failure detection and recovery
- **Graceful Degradation**: Continue operating with reduced functionality
- **Error Logging**: Comprehensive error logging and monitoring

## Performance Optimization

### HTTP API Optimization
- **Connection Pooling**: Reuse HTTP connections
- **Request Batching**: Batch multiple requests when possible
- **Rate Limit Management**: Intelligent rate limit handling
- **Timeout Management**: Configurable timeouts per operation

### WebSocket Optimization
- **Message Buffering**: Buffer messages during high load
- **Selective Subscriptions**: Subscribe only to required channels
- **Connection Management**: Efficient connection lifecycle management
- **Message Processing**: Multi-threaded message processing

### ZMQ Optimization
- **Message Serialization**: Efficient binary serialization
- **Publisher Optimization**: High-performance message publishing
- **Subscriber Optimization**: Efficient message subscription
- **Memory Management**: Optimized memory usage patterns

## Monitoring and Metrics

### Key Metrics
- **Order Execution Rate**: Orders per second
- **HTTP Request Rate**: API requests per second
- **WebSocket Message Rate**: Messages per second
- **Error Rates**: Error rates by type
- **Latency Metrics**: Order execution latency
- **Connection Status**: HTTP and WebSocket connection health

### Health Checks
- **HTTP API Health**: API connectivity and authentication
- **WebSocket Health**: WebSocket connection status
- **ZMQ Health**: Inter-process communication health
- **Process Health**: Overall process health status

### Logging
- **Centralized Logging System**: All `std::cout`/`std::cerr` replaced with logging macros
- **Structured Logging**: JSON-formatted logs with metadata
- **Log Levels**: DEBUG, INFO, WARN, ERROR
  - **DEBUG**: Normal trading flow (order operations, position updates)
  - **INFO**: Lifecycle events (startup, shutdown, configuration)
  - **WARN**: Warnings and degraded states
  - **ERROR**: Errors and failures
- **Performance Logs**: Detailed performance metrics
- **Error Logs**: Comprehensive error information

## Deployment

### Systemd Service
```bash
# Install systemd service
sudo cp trading_engine.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable trading_engine_binance
sudo systemctl start trading_engine_binance
```

### Manual Deployment
```bash
# Build from source
cd cpp/
mkdir build && cd build
cmake .. && make -j4

# Run trading engine
./bin/trading_engine BINANCE config/trading_engine_binance.ini --daemon
```

## Troubleshooting

### Common Issues

#### 1. HTTP API Authentication Errors
```bash
# Check API keys
grep "API_KEY" config/trading_engine_binance.ini

# Verify signature generation
./bin/trading_engine BINANCE --test-auth
```

#### 2. WebSocket Connection Issues
```bash
# Check WebSocket URL
grep "WS_PRIVATE_URL" config/trading_engine_binance.ini

# Test WebSocket connectivity
./bin/trading_engine BINANCE --test-websocket
```

#### 3. ZMQ Communication Issues
```bash
# Check ZMQ endpoints
grep "ENDPOINT" config/trading_engine_binance.ini

# Test ZMQ connectivity
./bin/zmq_test config/trading_engine_binance.ini
```

#### 4. Performance Issues
```bash
# Check resource usage
top -p $(pgrep trading_engine)

# Monitor metrics
tail -f logs/trading_engine_binance.log | grep "METRICS"
```

### Debug Mode
```bash
# Run in debug mode
./bin/trading_engine BINANCE config/trading_engine_binance.ini --log-level DEBUG

# Enable verbose logging
./bin/trading_engine BINANCE config/trading_engine_binance.ini --verbose
```

## Security Considerations

### API Key Security
- **Environment Variables**: Store API keys in environment variables
- **File Permissions**: Restrict access to configuration files
- **Key Rotation**: Regularly rotate API keys
- **Audit Logging**: Log all API key usage

### Network Security
- **TLS/SSL**: Use encrypted connections for HTTP API
- **WebSocket Security**: Secure WebSocket connections
- **Firewall Rules**: Restrict network access
- **VPN Usage**: Use VPN for production deployments

### Data Security
- **Message Encryption**: Encrypt sensitive data in ZMQ messages
- **Log Sanitization**: Remove sensitive data from logs
- **Access Control**: Implement proper access controls
- **Audit Trails**: Maintain comprehensive audit trails

## Best Practices

### Configuration Management
- **Version Control**: Track configuration changes
- **Validation**: Validate all configuration parameters
- **Documentation**: Document all configuration options
- **Testing**: Test configurations in development first

### Error Handling
- **Graceful Degradation**: Handle errors gracefully
- **Retry Logic**: Implement intelligent retry mechanisms
- **Circuit Breakers**: Use circuit breakers for external services
- **Monitoring**: Monitor error rates and patterns

### Performance
- **Resource Monitoring**: Monitor CPU, memory, and network usage
- **Optimization**: Continuously optimize performance
- **Scaling**: Plan for horizontal scaling
- **Load Testing**: Regular load testing

### Maintenance
- **Regular Updates**: Keep dependencies updated
- **Security Patches**: Apply security patches promptly
- **Backup Procedures**: Implement backup and recovery procedures
- **Documentation**: Maintain up-to-date documentation

This trading engine provides a robust, scalable foundation for executing trades and managing private data streams with comprehensive error handling, monitoring, and security features. However, production testing is required before live deployment.
