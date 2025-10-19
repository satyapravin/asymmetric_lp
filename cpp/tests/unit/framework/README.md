# Framework Unit Tests

This directory contains comprehensive unit tests for framework-level components using mock interfaces.

## 🎯 **Test Strategy**

### **Centralized Testing Approach**
All framework tests are located under `cpp/tests/unit/framework/` because:

1. **Framework components are shared** across all exchanges
2. **Mock interfaces** can be reused by all exchange tests  
3. **Easier maintenance** - one place for framework tests
4. **Better organization** - separates framework from exchange-specific tests

## 📁 **Test Structure**

```
cpp/tests/unit/framework/
├── interfaces/
│   └── mock_exchange_interfaces.hpp    # Mock implementations of exchange interfaces
├── config/
│   └── test_api_endpoint_manager.cpp   # Configuration management tests
├── utils/
│   ├── test_message_handler_manager.cpp    # Message handling tests
│   ├── test_market_data_normalizer.cpp      # Market data processing tests
│   └── test_exchange_monitor.cpp            # Exchange monitoring tests
├── zmq/
│   └── test_zmq_components.cpp          # ZMQ communication tests
├── websocket/
│   └── test_websocket_handler.cpp       # WebSocket framework tests
└── test_framework_runner.cpp            # Main test runner
```

## 🧪 **Mock Interfaces**

### **MockExchangeOMS**
- Mock implementation of `IExchangeOMS`
- Simulates order management operations
- Tracks order events and callbacks
- Test helper methods for verification

### **MockExchangePMS** 
- Mock implementation of `IExchangePMS`
- Simulates position management operations
- Tracks position updates and callbacks
- Test helper methods for verification

### **MockExchangeDataFetcher**
- Mock implementation of `IExchangeDataFetcher`
- Simulates HTTP data fetching operations
- Pre-configured test data
- Test helper methods for data setup

### **MockExchangeSubscriber**
- Mock implementation of `IExchangeSubscriber`
- Simulates market data subscription operations
- Tracks subscriptions and callbacks
- Test helper methods for data simulation

## 🔧 **Framework Components Tested**

### **1. Configuration Management**
- `ApiEndpointManager` - URL and endpoint management
- Configuration loading and validation
- URL resolution for different protocols
- Authentication configuration

### **2. Message Handling**
- `MessageHandlerManager` - Message handling framework
- Handler lifecycle management
- Callback mechanisms
- Error handling

### **3. Market Data Processing**
- `MarketDataNormalizer` - Market data processing
- Parser registration and management
- Message processing pipeline
- Callback mechanisms

### **4. Exchange Monitoring**
- `ExchangeMonitor` - Exchange health monitoring
- Performance metrics tracking
- Health status management
- Alert mechanisms

### **5. Communication**
- `ZMQPublisher/Subscriber` - Inter-process communication
- Connection management
- Message publishing/subscription
- Thread safety

### **6. WebSocket Framework**
- `IWebSocketHandler` - WebSocket abstraction
- Connection management
- Message handling
- Error handling

## 🚀 **Running Tests**

### **Build Tests**
```bash
cd /home/pravin/dev/asymmetric_lp/cpp/build
make run_tests
```

### **Run Framework Tests Only**
```bash
cd /home/pravin/dev/asymmetric_lp/cpp/build
./tests/run_tests --test-case="*Framework*"
```

### **Run Specific Component Tests**
```bash
# Configuration tests
./tests/run_tests --test-case="*ApiEndpointManager*"

# Message handling tests  
./tests/run_tests --test-case="*MessageHandlerManager*"

# Market data tests
./tests/run_tests --test-case="*MarketDataNormalizer*"

# Exchange monitoring tests
./tests/run_tests --test-case="*ExchangeMonitor*"

# ZMQ tests
./tests/run_tests --test-case="*ZMQ*"

# WebSocket tests
./tests/run_tests --test-case="*WebSocket*"
```

## 📊 **Test Coverage**

### **Interface Testing**
- ✅ Connection management
- ✅ Authentication
- ✅ Order operations
- ✅ Position operations
- ✅ Data fetching
- ✅ Market data subscription
- ✅ Callback mechanisms
- ✅ Error handling

### **Framework Testing**
- ✅ Configuration management
- ✅ Message handling
- ✅ Market data processing
- ✅ Exchange monitoring
- ✅ Communication protocols
- ✅ WebSocket framework
- ✅ Thread safety
- ✅ Performance

## 🎯 **Benefits**

1. **Isolated Testing**: Framework components tested independently
2. **Mock-Based**: No external dependencies or network calls
3. **Fast Execution**: Unit tests run quickly without I/O
4. **Comprehensive Coverage**: All framework functionality tested
5. **Reusable Mocks**: Can be used by exchange-specific tests
6. **Maintainable**: Clear separation of concerns

## 🔄 **Integration with Exchange Tests**

The mock interfaces can be used by exchange-specific tests:

```cpp
#include "unit/framework/interfaces/mock_exchange_interfaces.hpp"

TEST_CASE("Exchange Integration Test") {
    // Use mock interfaces for testing exchange integration
    auto mock_oms = std::make_unique<framework_tests::MockExchangeOMS>();
    auto mock_pms = std::make_unique<framework_tests::MockExchangePMS>();
    
    // Test exchange integration without real network calls
    // ...
}
```

This approach provides comprehensive testing coverage while maintaining fast execution and clear separation between framework and exchange-specific concerns.
