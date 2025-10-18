# Asymmetric LP Trading System Architecture Overview

## System Architecture

The Asymmetric LP trading system implements a sophisticated **dual-venue architecture** that combines **DeFi liquidity provision** (Python) with **CeFi market making** (C++) to create a statistical, inventory-aware hedge. The system consists of:

1. **Python DeFi LP Component** - Provides liquidity on Uniswap V3
2. **C++ CeFi Multi-Process Component** - Market makes on centralized exchanges
3. **ZMQ Integration Layer** - Connects both components via high-performance messaging

## Complete System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           PYTHON DEFI LP COMPONENT                             │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    Uniswap V3 Liquidity Provision                      │   │
│  │  - Avellaneda-Stoikov Model                                           │   │
│  │  - GLFT Model (Finite Inventory)                                      │   │
│  │  - Asymmetric Ranges                                                  │   │
│  │  - Edge-triggered Rebalancing                                         │   │
│  │  - Fee Tier Optimization                                              │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                        Inventory Management                            │   │
│  │  - Real-time Position Tracking                                        │   │
│  │  - USD Valuation and P&L                                              │   │
│  │  - Inventory Deviation Calculation                                    │   │
│  │  - Rebalance Signal Generation                                        │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                        ZMQ Publisher                                  │   │
│  │  PUBLISHES: Inventory Delta (6000) → CeFi Integration                 │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                │
                                │ Inventory Delta via ZMQ
                                ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           C++ CEFI COMPONENT                                   │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                           TRADER PROCESS                               │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    Strategy Engine                             │   │   │
│  │  │  - Market Making Strategy                                     │   │   │
│  │  │  - Risk Management                                             │   │   │
│  │  │  - Order Generation                                            │   │   │
│  │  │  - Inventory-Aware Hedging                                     │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                        ZMQ Communication                       │   │   │
│  │  │  PUBLISHES: Order Events (6002), Position Events (6003)       │   │   │
│  │  │  SUBSCRIBES: Market Data (7001), Positions (7002), Order Updates (7003)│   │   │
│  │  │  SUBSCRIBES: Inventory Delta (7000) ← DeFi Integration        │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                │                                               │
│                                │ ZMQ Messages                                  │
│                                ▼                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    EXCHANGE-SPECIFIC PROCESSES                         │   │
│  │                                                                         │   │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐      │   │
│  │  │ QUOTE SERVER    │  │ TRADING ENGINE  │  │ POSITION SERVER │      │   │
│  │  │ (Per Exchange)  │  │ (Per Exchange)  │  │ (Per Exchange)  │      │   │
│  │  │                 │  │                 │  │                 │      │   │
│  │  │ PUBLIC CHANNELS │  │ PRIVATE CHANNELS│  │ POSITION MGMT   │      │   │
│  │  │ - Market Data   │  │ - HTTP API      │  │ - Balance Mgmt  │      │   │
│  │  │ - WebSocket     │  │ - WebSocket     │  │ - PnL Calc      │      │   │
│  │  │ - Orderbook     │  │ - Order Exec    │  │                 │      │   │
│  │  │ - Ticker        │  │ - Account Data  │  │                 │      │   │
│  │  │                 │  │ - Balance Data  │  │                 │      │   │
│  │  │ PUB: 6001       │  │ PUB: 6002,6017  │  │ PUB: 6003,6011  │      │   │
│  │  │ SUB: 7001       │  │ SUB: 7003,7004  │  │ SUB: 7002,7004  │      │   │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────┘      │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                │
                                │ Exchange APIs
                                ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              EXCHANGES                                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │    BINANCE      │  │     DERIBIT     │  │      GRVT       │              │
│  │                 │  │                 │  │                 │              │
│  │ • Futures API   │  │ • Options API   │  │ • Perpetual API │              │
│  │ • WebSocket     │  │ • WebSocket     │  │ • WebSocket     │              │
│  │ • Spot API      │  │ • Spot API      │  │ • Spot API      │              │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Component Architecture

### 1. Python DeFi LP Component ✅ **Production Ready**

**Purpose**: Provides liquidity on Uniswap V3 using sophisticated mathematical models

**Location**: `python/`

**Key Features**:
- **Avellaneda-Stoikov Model**: Classic market-making model with optimal spreads
- **GLFT Model**: Finite inventory constraints with execution costs
- **Asymmetric Ranges**: Biased liquidity provision based on inventory position
- **Edge-triggered Rebalancing**: Efficient rebalance logic to minimize gas costs
- **Fee Tier Optimization**: Dynamic selection of 5 bps, 30 bps, or 100 bps fee tiers
- **Real-time Backtesting**: Comprehensive strategy validation and optimization

**Inventory Management**:
- **Real-time Position Tracking**: Continuous monitoring of LP positions
- **USD Valuation**: Real-time USD valuation of all positions
- **Inventory Deviation Calculation**: Quantifies deviation from target allocation
- **Rebalance Signal Generation**: Triggers rebalancing when thresholds exceeded

**ZMQ Integration**:
- **Inventory Publisher**: Publishes inventory delta to CeFi component
- **Port**: 6000 (Inventory Delta)
- **Message Format**: JSON with position data, USD values, and rebalance signals

**Configuration**: `python/config.py`

### 2. C++ CeFi Multi-Process Component ✅ **Complete**

**Purpose**: Market makes on centralized exchanges using residual inventory as a hedge

**Location**: `cpp/`

**Process Architecture**:

#### 2.1 Trader Process
**Purpose**: Central strategy engine that coordinates all trading activities

**Responsibilities**:
- Market making strategy implementation
- Risk management and position limits
- Order generation and decision making
- Inventory-aware hedging based on DeFi LP positions
- Coordination with all exchange processes

**ZMQ Communication**:
- **Publishes**: Order Events (6002), Position Events (6003), Strategy Events (6004)
- **Subscribes**: Market Data (7001), Positions (7002), Order Updates (7003), Inventory Delta (7000)

**Configuration**: `cpp/config/trader.ini`

#### 2.2 Quote Server (Per Exchange)
**Purpose**: Handles public market data streams from exchanges

**Responsibilities**:
- WebSocket connection to exchange public streams
- Market data processing (orderbook, ticker, trades)
- Data normalization and formatting
- ZMQ publishing of market data
- Connection management and reconnection

**ZMQ Communication**:
- **Publishes**: Market Data (6001), Orderbook (6005), Ticker (6006), Trades (6007)
- **Subscribes**: Trader Commands (7001)

**Configuration**: `cpp/config/quote_server_<EXCHANGE>.ini`

#### 2.3 Trading Engine (Per Exchange)
**Purpose**: Executes orders and manages private data streams

**Responsibilities**:
- HTTP API operations (order placement, cancellation, modification)
- Private WebSocket streams (order updates, account data)
- Order lifecycle management
- Authentication and signature management
- Rate limiting and error handling

**ZMQ Communication**:
- **Publishes**: Order Events (6002), Trade Events (6017), Order Status (6018)
- **Subscribes**: Trader Orders (7003), Position Updates (7004)

**Configuration**: `cpp/config/trading_engine_<EXCHANGE>.ini`

#### 2.4 Position Server (Per Exchange)
**Purpose**: Tracks positions, balances, and PnL calculations

**Responsibilities**:
- Real-time position tracking
- Balance management and updates
- PnL calculations and reporting
- Risk monitoring and alerts
- Position reconciliation

**ZMQ Communication**:
- **Publishes**: Position Events (6003), Balance Events (6011), PnL Events (6012)
- **Subscribes**: Trader Commands (7002), Trading Engine Updates (7004)

**Configuration**: `cpp/config/position_server_<EXCHANGE>.ini`

## Data Flow Architecture

### DeFi to CeFi Integration Flow
```
Python DeFi LP → Inventory Delta → ZMQ (6000) → C++ Trader Process
     ↓                                      ↓
Uniswap V3                            Exchange Processes
     ↓                                      ↓
ETH/USDC Pool                        Binance/Deribit/GRVT
     ↓                                      ↓
Fee Collection                        Order Execution
     ↓                                      ↓
P&L Tracking                          Position Updates
```

### Public Data Flow (Market Data)
```
Exchange WebSocket (Public) → Quote Server → ZMQ (6001) → Trader
```

### Private Data Flow (Trading Operations)
```
Trader → ZMQ (7003) → Trading Engine → Exchange HTTP API → Order Execution
Trader → ZMQ (7003) → Trading Engine → Exchange WebSocket (Private) → Order Updates
```

### Position Data Flow
```
Exchange API → Position Server → ZMQ (6003) → Trader
Trading Engine → ZMQ (6017) → Position Server → Position Updates
```

### Complete Integration Flow
```
DeFi LP (Python) → Inventory Delta → ZMQ → Trader Process (C++)
     ↓                                      ↓
Uniswap V3                            Exchange Processes
     ↓                                      ↓
ETH/USDC Pool                        Binance/Deribit/GRVT
     ↓                                      ↓
Fee Collection                        Order Execution
     ↓                                      ↓
P&L Tracking                          Position Updates
```

## ZMQ Communication Architecture

### Port Allocation Strategy

| Port Range | Purpose | Description |
|------------|---------|-------------|
| **6001-6099** | Publisher Endpoints | Processes publish to these ports |
| **7001-7099** | Subscriber Endpoints | Processes subscribe to these ports |

### Specific Port Assignments

#### Python DeFi LP Component
- **Publishes to**: 6000 (Inventory Delta)

#### C++ Trader Process
- **Publishes to**: 6002 (Order Events), 6003 (Position Events), 6004 (Strategy Events)
- **Subscribes to**: 7001 (Market Data), 7002 (Positions), 7003 (Order Updates), 7000 (Inventory Delta)

#### Quote Server (Per Exchange)
- **Publishes to**: 6001 (Market Data), 6005 (Orderbook), 6006 (Ticker), 6007 (Trades)
- **Subscribes to**: 7001 (Trader Commands)

#### Trading Engine (Per Exchange)
- **Publishes to**: 6002 (Order Events), 6017 (Trade Events), 6018 (Order Status)
- **Subscribes to**: 7003 (Trader Orders), 7004 (Position Updates)

#### Position Server (Per Exchange)
- **Publishes to**: 6003 (Position Events), 6011 (Balance Events), 6012 (PnL Events)
- **Subscribes to**: 7002 (Trader Commands), 7004 (Trading Engine Updates)

## Configuration Architecture

### Per-Process Configuration System

Each process has its own configuration file, providing:

- **Independent Configuration**: Each process can have different settings
- **Easy Maintenance**: Modify one process without affecting others
- **Scalability**: Add new exchanges by creating new config files
- **Flexibility**: Different environments can have different configs

### Configuration File Structure

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

## Exchange Integration Architecture

### Supported Exchanges

#### Binance
- **Asset Types**: Futures, Spot, Perpetual
- **APIs**: REST API, WebSocket Streams
- **Authentication**: API Key + Secret (HMAC-SHA256)
- **Rate Limits**: 1200 requests/minute, 10 requests/second

#### Deribit
- **Asset Types**: Options, Perpetual, Spot
- **APIs**: REST API, WebSocket Streams
- **Authentication**: API Key + Secret (HMAC-SHA256)
- **Rate Limits**: 20 requests/second

#### GRVT
- **Asset Types**: Perpetual, Spot
- **APIs**: REST API, WebSocket Streams
- **Authentication**: API Key + Secret (HMAC-SHA256)
- **Rate Limits**: 10 requests/second

### Exchange-Specific Implementations

Each exchange has its own implementation of:
- **HTTP Handlers**: Exchange-specific API endpoints and authentication
- **WebSocket Handlers**: Exchange-specific message formats and protocols
- **Data Parsers**: Exchange-specific data normalization
- **Rate Limiters**: Exchange-specific rate limit handling

## Security Architecture

### API Key Management
- **Environment Variables**: API keys stored in environment variables
- **File Permissions**: Restricted access to configuration files
- **Key Rotation**: Regular API key rotation
- **Audit Logging**: Comprehensive API key usage logging

### Network Security
- **TLS/SSL**: Encrypted connections for HTTP API
- **WebSocket Security**: Secure WebSocket connections
- **Firewall Rules**: Restricted network access
- **VPN Usage**: VPN for production deployments

### Data Security
- **Message Encryption**: Encrypted ZMQ messages for sensitive data
- **Log Sanitization**: Sensitive data removed from logs
- **Access Control**: Proper access controls implemented
- **Audit Trails**: Comprehensive audit trails maintained

## Performance Architecture

### Scalability Design
- **Horizontal Scaling**: Add more processes per exchange
- **Load Distribution**: Distribute load across multiple processes
- **Resource Isolation**: Each process has isolated resources
- **Independent Scaling**: Scale processes independently

### Performance Optimization
- **Connection Pooling**: Reuse HTTP connections
- **Message Buffering**: Buffer messages during high load
- **Multi-threading**: Multi-threaded message processing
- **Memory Management**: Optimized memory usage patterns

### Resource Management
- **CPU Limits**: Configurable CPU limits per process
- **Memory Limits**: Configurable memory limits per process
- **Network Limits**: Bandwidth and connection limits
- **Storage Limits**: Disk usage limits for logs and data

## Monitoring Architecture

### Health Monitoring
- **Process Health**: Individual process health checks
- **Connection Health**: HTTP and WebSocket connection monitoring
- **ZMQ Health**: Inter-process communication health
- **System Health**: Overall system health status

### Metrics Collection
- **Performance Metrics**: Order rates, latency, throughput
- **Error Metrics**: Error rates, types, and patterns
- **Resource Metrics**: CPU, memory, network usage
- **Business Metrics**: PnL, positions, trade counts

### Logging Architecture
- **Structured Logging**: JSON-formatted logs with metadata
- **Log Levels**: DEBUG, INFO, WARN, ERROR
- **Per-Process Logging**: Individual log files per process
- **Centralized Logging**: Optional centralized log aggregation

## Deployment Architecture

### Docker Deployment
- **Containerization**: Each process in its own container
- **Docker Compose**: Multi-service orchestration
- **Resource Limits**: CPU and memory constraints per container
- **Volume Mounts**: Persistent data and configuration storage

### Systemd Services
- **Service Management**: Individual systemd services per process
- **Auto-restart**: Automatic restart on failure
- **Dependency Management**: Service dependencies and startup order
- **Log Management**: Systemd journal integration

### Manual Deployment
- **Binary Deployment**: Direct binary execution
- **Process Management**: Manual process management
- **Configuration Management**: Manual configuration file management
- **Monitoring**: Manual monitoring and health checks

## Error Handling Architecture

### Error Types
- **Network Errors**: Connection timeouts and failures
- **Authentication Errors**: Invalid API keys or signatures
- **Rate Limit Errors**: API rate limit violations
- **Business Logic Errors**: Invalid orders or parameters

### Error Recovery
- **Retry Policies**: Configurable retry attempts with backoff
- **Circuit Breakers**: Automatic failure detection and recovery
- **Graceful Degradation**: Continue operating with reduced functionality
- **Error Propagation**: Proper error propagation between processes

### Error Monitoring
- **Error Logging**: Comprehensive error logging
- **Error Metrics**: Error rate and pattern monitoring
- **Alerting**: Automated error alerting
- **Recovery Tracking**: Error recovery success tracking

## Future Architecture Enhancements

### Planned Features
- **Advanced Analytics Dashboard**: Real-time monitoring UI
- **Machine Learning Integration**: AI-powered strategy optimization
- **Multi-Asset Support**: Additional trading pairs and assets
- **Advanced Order Types**: Complex order strategies and algorithms

### Scalability Improvements
- **Microservices Architecture**: Further process decomposition
- **Message Queuing**: Advanced message queuing systems
- **Database Integration**: Persistent data storage
- **API Gateway**: Centralized API management

### Security Enhancements
- **Advanced Authentication**: Multi-factor authentication
- **Encryption**: End-to-end encryption
- **Compliance**: Regulatory compliance features
- **Audit**: Advanced audit and compliance reporting

This architecture provides a robust, scalable foundation for high-frequency trading with clear separation of concerns, comprehensive error handling, and extensive monitoring capabilities.
