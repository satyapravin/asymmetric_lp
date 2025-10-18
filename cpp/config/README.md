# Trading System Configuration Guide

This document explains the per-process configuration system for the trading system.

## Overview

Each process in the trading system has its own configuration file, allowing for:
- **Independent configuration** - Each process can have different settings
- **Easy maintenance** - Modify one process without affecting others
- **Scalability** - Add new exchanges by creating new config files
- **Flexibility** - Different environments (dev, staging, production) can have different configs

## Configuration File Structure

### Process Types and Config Files

| Process Type | Config File | Description |
|--------------|-------------|-------------|
| **Trader** | `config/trader.ini` | Main strategy process configuration |
| **Quote Server** | `config/quote_server_<EXCHANGE>.ini` | Market data server per exchange |
| **Trading Engine** | `config/trading_engine_<EXCHANGE>.ini` | Order execution engine per exchange |
| **Position Server** | `config/position_server_<EXCHANGE>.ini` | Position tracking server per exchange |

### Example Files

```
config/
├── trader.ini                           # Trader process config
├── quote_server_binance.ini            # Binance quote server
├── quote_server_deribit.ini            # Deribit quote server
├── trading_engine_binance.ini          # Binance trading engine
├── trading_engine_deribit.ini         # Deribit trading engine
├── position_server_binance.ini        # Binance position server
├── position_server_deribit.ini        # Deribit position server
└── templates/                          # Template files
    ├── quote_server_template.ini
    ├── trading_engine_template.ini
    └── position_server_template.ini
```

## Configuration Sections

### Common Sections (All Processes)

#### `[GLOBAL]`
- `PROCESS_NAME` - Name of the process
- `EXCHANGE_NAME` - Exchange name (for exchange-specific processes)
- `LOG_LEVEL` - Logging level (DEBUG, INFO, WARNING, ERROR)
- `CONFIG_VERSION` - Configuration file version
- `ENVIRONMENT` - Environment (development, staging, production)
- `PID_FILE` - Process ID file location
- `LOG_FILE` - Log file location

#### `[PUBLISHERS]` / `[SUBSCRIBERS]`
- ZMQ endpoint configurations for inter-process communication
- Each endpoint has a unique port number
- Format: `tcp://127.0.0.1:PORT`

#### `[PERFORMANCE]`
- Threading configuration
- Memory management settings
- Message processing parameters

#### `[MONITORING]`
- Metrics collection settings
- Health check intervals
- Alerting configuration

### Process-Specific Sections

#### Trader Process (`trader.ini`)
```ini
[GLOBAL]
PROCESS_NAME=trading_strategy
LOG_LEVEL=INFO

[PUBLISHERS]
ORDER_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6002
POSITION_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6003

[SUBSCRIBERS]
QUOTE_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7001
POSITION_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7002
TRADING_ENGINE_SUB_ENDPOINT=tcp://127.0.0.1:7003

[EXCHANGE_BINANCE]
ENABLED=true
EXCHANGE_NAME=BINANCE
SYMBOLS=BTCUSDT,ETHUSDT,ADAUSDT
MAX_ORDERS_PER_SYMBOL=10
```

#### Quote Server (`quote_server_<EXCHANGE>.ini`)
```ini
[GLOBAL]
PROCESS_NAME=quote_server_binance
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=your_binance_api_key
API_SECRET=your_binance_api_secret

[WEBSOCKET]
WS_PUBLIC_URL=wss://fstream.binance.com/ws
WS_RECONNECT_INTERVAL_MS=5000
WS_PING_INTERVAL_MS=30000

[MARKET_DATA]
SYMBOLS=BTCUSDT,ETHUSDT,ADAUSDT,BNBUSDT,SOLUSDT
COLLECT_TICKER=true
COLLECT_ORDERBOOK=true
COLLECT_TRADES=true
```

#### Trading Engine (`trading_engine_<EXCHANGE>.ini`)
```ini
[GLOBAL]
PROCESS_NAME=trading_engine_binance
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=your_binance_api_key
API_SECRET=your_binance_api_secret

[ORDER_MANAGEMENT]
MAX_ORDERS_PER_SECOND=10
ORDER_TIMEOUT_MS=5000
RETRY_FAILED_ORDERS=true
MAX_ORDER_RETRIES=3

[RISK_MANAGEMENT]
MAX_POSITION_SIZE=10.0
MAX_POSITION_PER_SYMBOL=5.0
ENABLE_PRE_TRADE_CHECKS=true
```

#### Position Server (`position_server_<EXCHANGE>.ini`)
```ini
[GLOBAL]
PROCESS_NAME=position_server_binance
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=your_binance_api_key
API_SECRET=your_binance_api_secret

[POSITION_MANAGEMENT]
POSITION_UPDATE_INTERVAL_MS=1000
BALANCE_UPDATE_INTERVAL_MS=5000
PNL_CALCULATION_INTERVAL_MS=1000

[RISK_MANAGEMENT]
MAX_POSITION_SIZE=10.0
MAX_POSITION_PER_SYMBOL=5.0
MAX_DAILY_LOSS=5000.0
```

## ZMQ Communication Ports

### Port Allocation Strategy

| Port Range | Purpose | Description |
|------------|---------|-------------|
| **6001-6099** | Publisher Endpoints | Processes publish to these ports |
| **7001-7099** | Subscriber Endpoints | Processes subscribe to these ports |

### Specific Port Assignments

#### Trader Process
- **Publishes to:** 6002 (Order Events), 6003 (Position Events), 6004 (Strategy Events)
- **Subscribes to:** 7001 (Market Data), 7002 (Positions), 7003 (Order Updates)

#### Quote Server (Per Exchange)
- **Publishes to:** 6001 (Market Data), 6005 (Orderbook), 6006 (Ticker), 6007 (Trades)
- **Subscribes to:** 7001 (Trader Commands)

#### Trading Engine (Per Exchange)
- **Publishes to:** 6002 (Order Events), 6017 (Trade Events), 6018 (Order Status)
- **Subscribes to:** 7003 (Trader Orders), 7004 (Position Updates)

#### Position Server (Per Exchange)
- **Publishes to:** 6003 (Position Events), 6011 (Balance Events), 6012 (PnL Events)
- **Subscribes to:** 7002 (Trader Commands), 7004 (Trading Engine Updates)

## Configuration Management

### Loading Configurations

```cpp
#include "utils/config/process_config_manager.hpp"

// Load trader configuration
auto trader_config = config::ProcessConfigManager::load_trader_config();

// Load exchange-specific configurations
auto binance_quote_config = config::ProcessConfigManager::load_quote_server_config("BINANCE");
auto binance_engine_config = config::ProcessConfigManager::load_trading_engine_config("BINANCE");
auto binance_position_config = config::ProcessConfigManager::load_position_server_config("BINANCE");
```

### Accessing Configuration Values

```cpp
// Get string values
std::string log_level = config.get_string("GLOBAL", "LOG_LEVEL", "INFO");
std::string api_key = config.get_string("GLOBAL", "API_KEY", "");

// Get numeric values
int max_threads = config.get_int("PERFORMANCE", "MAX_WORKER_THREADS", 4);
double max_position = config.get_double("RISK_MANAGEMENT", "MAX_POSITION_SIZE", 10.0);

// Get boolean values
bool enable_metrics = config.get_bool("MONITORING", "ENABLE_METRICS", true);

// Get ZMQ endpoints
std::string pub_endpoint = config.get_publisher_endpoint("ORDER_EVENTS_PUB_ENDPOINT");
std::string sub_endpoint = config.get_subscriber_endpoint("TRADER_SUB_ENDPOINT");
```

### Configuration Validation

```cpp
// Validate configuration
if (!config.validate_config()) {
    auto errors = config.get_validation_errors();
    for (const auto& error : errors) {
        std::cerr << "Config error: " << error << std::endl;
    }
    return false;
}
```

## Process Startup

### Using the Startup Script

```bash
# Start all processes
./scripts/start_trading_system.sh start

# Stop all processes
./scripts/start_trading_system.sh stop

# Restart all processes
./scripts/start_trading_system.sh restart

# Check process status
./scripts/start_trading_system.sh status

# Validate configurations
./scripts/start_trading_system.sh validate
```

### Manual Process Startup

```bash
# Start individual processes
./bin/trader config/trader.ini &
./bin/quote_server BINANCE config/quote_server_binance.ini &
./bin/trading_engine BINANCE config/trading_engine_binance.ini --daemon &
./bin/position_server BINANCE config/position_server_binance.ini &
```

## Environment-Specific Configurations

### Development Environment
```ini
[GLOBAL]
ENVIRONMENT=development
LOG_LEVEL=DEBUG
TESTNET_MODE=true
```

### Production Environment
```ini
[GLOBAL]
ENVIRONMENT=production
LOG_LEVEL=INFO
TESTNET_MODE=false
```

## Best Practices

### 1. Configuration Organization
- Use descriptive section names
- Group related settings together
- Include comments for complex settings
- Use consistent naming conventions

### 2. Security
- Never commit API keys to version control
- Use environment variables for sensitive data
- Implement configuration encryption for production
- Regularly rotate API credentials

### 3. Validation
- Validate all configuration files before startup
- Implement range checks for numeric values
- Validate ZMQ endpoint formats
- Check for required sections and keys

### 4. Monitoring
- Enable metrics collection
- Set up health checks
- Configure alerting thresholds
- Monitor configuration changes

### 5. Maintenance
- Version control configuration files
- Document configuration changes
- Test configurations in development first
- Keep backup configurations

## Troubleshooting

### Common Issues

#### 1. Configuration File Not Found
```
Error: Config file not found: config/trader.ini
```
**Solution:** Ensure the configuration file exists and the path is correct.

#### 2. Invalid Configuration Values
```
Error: Invalid value for MAX_ORDERS_PER_SECOND: -1
```
**Solution:** Check value ranges and ensure positive values where required.

#### 3. ZMQ Port Conflicts
```
Error: Address already in use: tcp://127.0.0.1:6002
```
**Solution:** Check for duplicate port assignments across configuration files.

#### 4. Missing Required Sections
```
Error: Required section [GLOBAL] not found
```
**Solution:** Ensure all required sections are present in the configuration file.

### Debugging Configuration Issues

```bash
# Validate all configurations
./scripts/start_trading_system.sh validate

# Check specific configuration
./bin/config_validator config/trader.ini

# Test ZMQ connectivity
./bin/zmq_test config/trader.ini
```

This configuration system provides a robust, scalable foundation for managing the trading system's processes with clear separation of concerns and easy maintenance.
