# Binance Testnet Integration Tests

This directory contains integration tests for Binance testnet using real configuration files and credentials.

## 🎯 **Test Strategy**

### **Configuration-Based Testing**
All tests use the configuration system instead of hardcoded values:

1. **Configuration Files**: Load URLs and settings from `binance_config.json`
2. **Credential Management**: Load API keys from config file or environment variables
3. **Test Symbol**: Use BTCUSDT for all testnet operations
4. **Real Network Calls**: Test actual connectivity to Binance testnet

## 📁 **Test Structure**

```
cpp/tests/unit/framework/integration/
├── test_binance_testnet_integration.cpp    # Main integration tests
├── test_config_loader.hpp                  # Configuration loader utility
└── README.md                               # This file
```

## 🔧 **Configuration Setup**

### **Binance Testnet Configuration**
The `binance_config.json` file contains:

```json
{
  "exchange_name": "BINANCE",
  "testnet_mode": true,
  "assets": {
    "FUTURES": {
      "urls": {
        "rest_api": "https://testnet.binancefuture.com",
        "websocket_public": "wss://dstream.binancefuture.com/stream",
        "websocket_private": "wss://dstream.binancefuture.com/ws"
      }
    }
  },
  "authentication": {
    "testnet_api_key": "yf482RhkdiQg4S5Oaau8vGqPlimMn5Rlws1OvN3ThkJb2H7BXQJNx3KVH0KgbNxe",
    "testnet_secret": "a90vC7XnrCLXprjIfsnCPWbjijNjtY0zTiAeTR9prEkOGS2Lk1LG5cBgtnXZkreG"
  }
}
```

### **Test Symbol**
- **Symbol**: BTCUSDT
- **Asset Type**: FUTURES
- **Test Operations**: Orderbook, trades, orders, positions

## 🧪 **Test Components**

### **1. Configuration Management Tests**
- Load Binance testnet configuration
- Verify testnet URLs and settings
- Test authentication configuration

### **2. HTTP Data Fetcher Tests**
- Initialize with testnet configuration
- Test BTCUSDT data fetching
- Verify authentication

### **3. Public WebSocket Subscriber Tests**
- Connect to testnet WebSocket
- Subscribe to BTCUSDT orderbook and trades
- Test market data callbacks

### **4. Private WebSocket OMS Tests**
- Connect to testnet private WebSocket
- Test BTCUSDT order operations
- Test order status callbacks

### **5. Private WebSocket PMS Tests**
- Connect to testnet private WebSocket
- Test BTCUSDT position updates
- Test position callbacks

### **6. End-to-End Integration Tests**
- Complete BTCUSDT trading workflow
- All components working together
- Real-time data flow testing

## 🚀 **Running Tests**

### **Prerequisites**
1. **Network Access**: Tests require internet connection to Binance testnet
2. **Valid Credentials**: Testnet API key and secret must be valid
3. **Testnet Account**: Binance testnet account with BTCUSDT trading enabled

### **Build and Run**
```bash
cd /home/pravin/dev/asymmetric_lp/cpp/build
make run_tests
./tests/run_tests --test-case="*Binance*Testnet*"
```

### **Run Specific Tests**
```bash
# Configuration tests
./tests/run_tests --test-case="*Configuration*Based*"

# Data fetcher tests
./tests/run_tests --test-case="*HTTP*Data*Fetcher*"

# WebSocket subscriber tests
./tests/run_tests --test-case="*Public*WebSocket*Subscriber*"

# OMS tests
./tests/run_tests --test-case="*Private*WebSocket*OMS*"

# PMS tests
./tests/run_tests --test-case="*Private*WebSocket*PMS*"

# End-to-end tests
./tests/run_tests --test-case="*End-to-End*Integration*"
```

## 🔐 **Credential Management**

### **Configuration File Method**
Credentials are loaded from `binance_config.json`:
```cpp
TestConfigLoader config_loader("../exchanges/binance/binance_config.json");
bool loaded = config_loader.load_testnet_credentials();
```

### **Environment Variable Method**
Fallback to environment variables:
```bash
export BINANCE_TESTNET_API_KEY="yf482RhkdiQg4S5Oaau8vGqPlimMn5Rlws1OvN3ThkJb2H7BXQJNx3KVH0KgbNxe"
export BINANCE_TESTNET_SECRET="a90vC7XnrCLXprjIfsnCPWbjijNjtY0zTiAeTR9prEkOGS2Lk1LG5cBgtnXZkreG"
```

### **Test Safety**
- Tests automatically skip if credentials are not available
- Uses `SKIP()` macro to gracefully handle missing credentials
- No hardcoded credentials in test code

## 📊 **Test Coverage**

### **Configuration System**
- ✅ URL resolution from config files
- ✅ Authentication configuration loading
- ✅ Testnet mode verification
- ✅ Channel name mapping

### **HTTP Operations**
- ✅ REST API connectivity
- ✅ Authentication with testnet credentials
- ✅ BTCUSDT data fetching
- ✅ Account information retrieval

### **WebSocket Operations**
- ✅ Public WebSocket connectivity
- ✅ Private WebSocket connectivity
- ✅ BTCUSDT market data subscription
- ✅ Real-time data callbacks

### **Order Management**
- ✅ BTCUSDT order placement
- ✅ Order status tracking
- ✅ Order cancellation
- ✅ Order modification

### **Position Management**
- ✅ BTCUSDT position updates
- ✅ Real-time position callbacks
- ✅ Account balance tracking

## 🎯 **Benefits**

1. **Real Integration Testing**: Tests actual connectivity to Binance testnet
2. **Configuration-Driven**: No hardcoded values, uses config system
3. **Comprehensive Coverage**: All exchange interfaces tested
4. **Safe Testing**: Uses testnet, not production
5. **BTCUSDT Focus**: Consistent test symbol across all operations
6. **Credential Security**: Credentials loaded from config, not hardcoded

## ⚠️ **Important Notes**

1. **Testnet Only**: These tests use Binance testnet, not production
2. **Network Dependent**: Tests require internet connection
3. **Credential Required**: Valid testnet API credentials needed
4. **Real Data**: Tests use real market data from testnet
5. **Rate Limits**: Respects Binance testnet rate limits

## 🔄 **Integration with Framework Tests**

These integration tests complement the framework unit tests:

- **Framework Tests**: Mock interfaces, fast execution, no network
- **Integration Tests**: Real interfaces, network calls, comprehensive testing

Together, they provide complete test coverage for the exchange system.
