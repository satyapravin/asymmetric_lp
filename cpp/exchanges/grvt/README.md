# GRVT Exchange Integration

This document describes the GRVT exchange integration for the Asymmetric Liquidity Provision Strategy system.

## Overview

GRVT is a decentralized derivatives exchange built on Ethereum Layer 2. Our integration provides:

- **Public WebSocket**: Market data streams (orderbook, trades, ticker)
- **Private WebSocket**: User data streams (orders, positions, balances) via JSON-RPC
- **HTTP REST API**: Order management, account data, and market data

## Architecture

### Components

1. **GrvtPublicWebSocketHandler**: Handles public market data streams
2. **GrvtPrivateWebSocketHandler**: Handles private user data streams with authentication
3. **GrvtDataFetcher**: HTTP REST API client for data retrieval
4. **GrvtOMS**: Order Management System for trading operations
5. **GrvtManager**: Exchange manager for quote server integration

### Directory Structure

```
cpp/exchanges/grvt/
├── public_websocket/
│   ├── grvt_public_websocket_handler.hpp
│   └── grvt_public_websocket_handler.cpp
├── private_websocket/
│   ├── grvt_private_websocket_handler.hpp
│   └── grvt_private_websocket_handler.cpp
├── http/
│   ├── grvt_data_fetcher.hpp
│   ├── grvt_data_fetcher.cpp
│   ├── grvt_oms.hpp
│   └── grvt_oms.cpp
├── grvt_manager.hpp
└── grvt_manager.cpp
```

## Authentication

GRVT uses a unique authentication system:

1. **API Key**: Generated through GRVT UI
2. **Session Cookie**: Obtained via authentication endpoint
3. **Account ID**: Retrieved during authentication process

### Authentication Flow

```cpp
// 1. Authenticate to get session cookie and account ID
std::string session_cookie = authenticate_with_api_key(api_key);
std::string account_id = get_account_id_from_response();

// 2. Use credentials for private connections
GrvtPrivateWebSocketHandler handler(api_key, session_cookie, account_id);
GrvtDataFetcher fetcher(api_key, session_cookie, account_id);
GrvtOMS oms(config);
```

## Configuration

### Quote Server Configuration

```ini
[GRVT]
WEBSOCKET_URL=wss://trades.grvt.io/ws/full
CHANNEL=orderbook.BTCUSDT
CHANNEL=trades.BTCUSDT
CHANNEL=ticker.BTCUSDT
SYMBOL=BTCUSDT
SYMBOL=ETHUSDT
SYMBOL=SOLUSDT
USE_LITE_VERSION=false
TESTNET=false
```

### Trading Engine Configuration

```ini
[GRVT]
API_KEY=${GRVT_API_KEY}
SESSION_COOKIE=${GRVT_SESSION_COOKIE}
ACCOUNT_ID=${GRVT_ACCOUNT_ID}
HTTP_URL=https://trades.grvt.io/full
PRIVATE_WS_URL=wss://trades.grvt.io/ws/full
PUBLIC_WS_URL=wss://trades.grvt.io/ws/full
SYMBOL=BTCUSDT
USE_LITE_VERSION=false
TESTNET=false
```

### Position Server Configuration

```ini
[GRVT]
API_KEY=
SESSION_COOKIE=
ACCOUNT_ID=
BASE_URL=https://trades.grvt.io/full
SYMBOLS=BTCUSDT,ETHUSDT,SOLUSDT
USE_LITE_VERSION=false
TESTNET=false
```

## API Endpoints

### WebSocket Endpoints

- **Public**: `wss://trades.grvt.io/ws/full` (Full version)
- **Public Lite**: `wss://trades.grvt.io/ws/lite` (Lite version)
- **Private**: `wss://trades.grvt.io/ws/full` (with authentication)

### HTTP Endpoints

- **Full API**: `https://trades.grvt.io/full/v1/`
- **Lite API**: `https://trades.grvt.io/lite/v1/`

## Message Formats

### JSON-RPC (Full Version)

```json
{
  "jsonrpc": "2.0",
  "method": "v1/aggregated_account_summary",
  "params": {},
  "id": 123
}
```

### JSON-RPC (Lite Version)

```json
{
  "j": "2.0",
  "m": "v1/aggregated_account_summary",
  "p": {},
  "i": 123
}
```

### WebSocket Subscriptions

```json
{
  "method": "SUBSCRIBE",
  "params": ["orderbook.BTCUSDT", "trades.BTCUSDT", "ticker.BTCUSDT"],
  "id": 1
}
```

## Order Management

### Order Types Supported

- **Market Orders**: Immediate execution at current market price
- **Limit Orders**: Execute at specified price or better
- **Stop Orders**: Triggered when price reaches stop level
- **Stop-Limit Orders**: Combination of stop and limit orders

### Order Signing

GRVT requires order signing with private keys:

```cpp
std::string signature = sign_order(order);
order_json["signature"] = signature;
```

## Market Data

### Supported Data Types

1. **Orderbook**: Real-time order book updates
2. **Trades**: Recent trade executions
3. **Ticker**: 24h price and volume statistics
4. **Market Data**: Combined market information

### Symbol Format

GRVT uses standard trading pair format:
- `BTCUSDT` - Bitcoin/USDT perpetual
- `ETHUSDT` - Ethereum/USDT perpetual
- `SOLUSDT` - Solana/USDT perpetual

## Error Handling

### Common Error Scenarios

1. **Authentication Failures**: Invalid API key, expired session cookie
2. **Rate Limiting**: Too many requests per second
3. **Invalid Orders**: Incorrect parameters, insufficient balance
4. **Network Issues**: Connection timeouts, WebSocket disconnections

### Error Response Format

```json
{
  "error": "Invalid API key",
  "code": 4001,
  "message": "Authentication failed"
}
```

## Testing

### Test Configuration

Create a test configuration file:

```ini
[GRVT]
API_KEY=test_api_key
SESSION_COOKIE=test_session_cookie
ACCOUNT_ID=test_account_id
TESTNET=true
USE_LITE_VERSION=false
```

### Mock Responses

The implementation includes mock responses for testing:

- Account info with sample balances
- Position data with mock PnL
- Order history with various statuses
- Market data with realistic prices

## Environment Variables

Set the following environment variables for production:

```bash
export GRVT_API_KEY="your_api_key_here"
export GRVT_SESSION_COOKIE="your_session_cookie_here"
export GRVT_ACCOUNT_ID="your_account_id_here"
```

## Security Considerations

1. **API Key Protection**: Store API keys securely, never commit to version control
2. **Session Management**: Refresh session cookies regularly (every 30 minutes)
3. **IP Whitelisting**: Configure IP whitelisting in GRVT UI for additional security
4. **Order Signing**: Use secure key management for order signing

## Performance

### Optimizations

1. **Connection Pooling**: Reuse HTTP connections for REST API calls
2. **WebSocket Compression**: Enable compression for WebSocket streams
3. **Batch Operations**: Group multiple operations when possible
4. **Caching**: Cache frequently accessed data (exchange info, symbols)

### Rate Limits

- **REST API**: 1200 requests per minute
- **WebSocket**: 10 connections per API key
- **Order Placement**: 10 orders per second

## Troubleshooting

### Common Issues

1. **Connection Failures**: Check network connectivity and firewall settings
2. **Authentication Errors**: Verify API key and session cookie validity
3. **Order Rejections**: Check order parameters and account balance
4. **WebSocket Disconnections**: Implement reconnection logic with exponential backoff

### Debug Mode

Enable debug logging:

```ini
LOG_LEVEL=DEBUG
```

### Health Checks

Monitor the following metrics:

- WebSocket connection status
- API response times
- Order fill rates
- Error rates by endpoint

## Future Enhancements

1. **Advanced Order Types**: Iceberg orders, TWAP orders
2. **Risk Management**: Position limits, exposure monitoring
3. **Analytics**: Trade analytics, performance metrics
4. **Multi-Account**: Support for multiple trading accounts

## Support

For issues related to GRVT integration:

1. Check GRVT API documentation: https://api-docs.grvt.io/
2. Review system logs for error messages
3. Verify configuration parameters
4. Test with GRVT testnet first

## References

- [GRVT API Documentation](https://api-docs.grvt.io/)
- [GRVT Help Center](https://help.grvt.io/)
- [GRVT Python SDK](https://pypi.org/project/grvt-pysdk/)
- [GRVT TypeScript SDK](https://www.npmjs.com/package/@grvt/sdk)
