# Asymmetric LP - Production Deployment Guide

## 🚀 **Production-Ready Dual-Venue Trading System**

### **COMPLETED FEATURES**

#### ✅ **1. Dual-Venue Architecture**
- **Python DeFi LP Component**: Uniswap V3 liquidity provision with sophisticated models
- **C++ CeFi Multi-Process Component**: Centralized exchange market making
- **ZMQ Integration Layer**: High-performance inter-component communication
- **Inventory-Aware Hedging**: Statistical hedge using residual inventory

#### ✅ **2. Python DeFi LP Component**
- **Avellaneda-Stoikov Model**: Classic market-making model implementation
- **GLFT Model**: Finite inventory constraints with execution costs
- **Asymmetric Ranges**: Biased liquidity provision based on inventory
- **Edge-triggered Rebalancing**: Efficient rebalance logic
- **Fee Tier Optimization**: Dynamic 5 bps, 30 bps, 100 bps selection
- **Real-time Backtesting**: Comprehensive strategy validation
- **ZMQ Integration**: Inventory delta publishing to CeFi component

#### ✅ **3. C++ Multi-Process Architecture**
- **Trader Process**: Strategy engine with inventory-aware hedging
- **Quote Server (Per Exchange)**: Public market data streams
- **Trading Engine (Per Exchange)**: Private trading operations (HTTP + WebSocket)
- **Position Server (Per Exchange)**: Position and balance management
- **ZMQ Communication**: High-performance inter-process messaging
- **Per-Process Configuration**: Independent configuration per process

#### ✅ **4. Logging & Monitoring**
- **Structured Logging**: JSON-formatted logs with metadata
- **Log Levels**: DEBUG, INFO, WARN, ERROR with configurable levels
- **Per-Process Logging**: Individual log files per process
- **Performance Metrics**: Built-in metrics collection per process
- **Health Monitoring**: Process health checks and status reporting

#### ✅ **5. Error Handling & Resilience**
- **Process Isolation**: Failure in one process doesn't affect others
- **Automatic Restart**: Failed processes automatically restart
- **Circuit Breakers**: Automatic failure detection and recovery
- **Retry Policies**: Exponential backoff with configurable limits
- **Graceful Degradation**: System continues operating with reduced functionality

#### ✅ **6. Deployment & Operations**
- **Docker Containerization**: Production-ready Docker setup
- **Docker Compose**: Multi-service orchestration
- **Process Management**: Individual process control and monitoring
- **Resource Limits**: CPU and memory constraints per process
- **Signal Handling**: Graceful shutdown on SIGTERM/SIGINT

---

## 🏗️ **Quick Start**

### **Prerequisites**
```bash
# Install Python dependencies
sudo apt-get update
sudo apt-get install python3 python3-pip python3-venv

# Install Docker and Docker Compose
sudo apt-get install docker.io docker-compose

# Install C++ build tools
sudo apt-get install build-essential cmake

# Add user to docker group
sudo usermod -aG docker $USER
```

### **1. Clone and Setup**
```bash
git clone <repository-url>
cd asymmetric_lp

# Setup Python environment
cd python/
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Build C++ components
cd ../cpp/
mkdir build && cd build
cmake .. && make -j4
```

### **2. Configure Environment**
```bash
# Create environment file
cat > .env << EOF
# Exchange API Keys (REQUIRED)
BINANCE_API_KEY=your_binance_api_key
BINANCE_API_SECRET=your_binance_api_secret
DERIBIT_API_KEY=your_deribit_api_key
DERIBIT_API_SECRET=your_deribit_api_secret
GRVT_API_KEY=your_grvt_api_key
GRVT_API_SECRET=your_grvt_api_secret

# Python DeFi Configuration
ETHEREUM_RPC_URL=https://mainnet.infura.io/v3/your_project_id
WALLET_PRIVATE_KEY=your_wallet_private_key
UNISWAP_V3_ROUTER_ADDRESS=0xE592427A0AEce92De3Edee1F18E0157C05861564

# Optional
LOG_LEVEL=INFO
ENVIRONMENT=production
EOF
```

### **3. Configure Components**
```bash
# Configure Python DeFi LP
cd python/
cp config.py.example config.py
# Edit config.py with your parameters

# Configure C++ processes
cd ../cpp/
cp config/trader.ini.example config/trader.ini
cp config/quote_server_binance.ini.example config/quote_server_binance.ini
cp config/trading_engine_binance.ini.example config/trading_engine_binance.ini
cp config/position_server_binance.ini.example config/position_server_binance.ini

# Update API keys in config files
sed -i 's/your_binance_api_key/'"$BINANCE_API_KEY"'/g' config/*binance*.ini
sed -i 's/your_binance_api_secret/'"$BINANCE_API_SECRET"'/g' config/*binance*.ini
```

### **4. Start Trading System**
```bash
# Start Python DeFi LP (Terminal 1)
cd python/
source venv/bin/activate
python main.py --config config.py &

# Start C++ CeFi processes (Terminal 2)
cd ../cpp/build/
./bin/quote_server BINANCE ../config/quote_server_binance.ini &
./bin/trading_engine BINANCE ../config/trading_engine_binance.ini --daemon &
./bin/position_server BINANCE ../config/position_server_binance.ini &
./bin/trader ../config/trader.ini &

# View logs
tail -f logs/python_lp.log
tail -f logs/trader.log
tail -f logs/quote_server_binance.log
tail -f logs/trading_engine_binance.log
tail -f logs/position_server_binance.log
```

---

## ⚙️ **Configuration**

### **Python DeFi LP Configuration**
The Python component uses `python/config.py` for configuration:

```python
# Trading Parameters
BASE_SPREAD = 0.02          # 2% base spread
REBALANCE_THRESHOLD = 0.10  # 10% inventory threshold
FEE_TIER = 0.0005          # 5 bps fee tier
MODEL_TYPE = "GLFT"        # GLFT or AS model

# Uniswap V3 Configuration
UNISWAP_V3_ROUTER = "0xE592427A0AEce92De3Edee1F18E0157C05861564"
WETH_ADDRESS = "0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2"
USDC_ADDRESS = "0xA0b86a33E6441b8c4C8C0e4b8b4c4C0e4b8b4c4C"

# Ethereum Configuration
ETHEREUM_RPC_URL = "https://mainnet.infura.io/v3/your_project_id"
WALLET_PRIVATE_KEY = "your_wallet_private_key"

# ZMQ Configuration
ZMQ_PUBLISHER_PORT = 6000   # Inventory delta publishing
ZMQ_PUBLISHER_HOST = "127.0.0.1"

# Risk Management
MAX_POSITION_SIZE = 10.0    # Maximum position size in ETH
MIN_LIQUIDITY = 0.1         # Minimum liquidity threshold
MAX_SLIPPAGE = 0.005        # Maximum slippage tolerance
```

### **C++ CeFi Configuration (Per-Process)**

#### **Trader Configuration** (`cpp/config/trader.ini`)
```ini
[GLOBAL]
PROCESS_NAME=trading_strategy
LOG_LEVEL=INFO
ENABLED_EXCHANGES=BINANCE,DERIBIT,GRVT

[PUBLISHERS]
ORDER_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6002
POSITION_EVENTS_PUB_ENDPOINT=tcp://127.0.0.1:6003

[SUBSCRIBERS]
QUOTE_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7001
TRADING_ENGINE_SUB_ENDPOINT=tcp://127.0.0.1:7003
POSITION_SERVER_SUB_ENDPOINT=tcp://127.0.0.1:7002
```

#### **Trading Engine Configuration** (`cpp/config/trading_engine_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures
API_KEY=${BINANCE_API_KEY}
API_SECRET=${BINANCE_API_SECRET}

[HTTP_API]
HTTP_BASE_URL=https://fapi.binance.com
HTTP_TIMEOUT_MS=5000
API_REQUESTS_PER_SECOND=10

[WEBSOCKET]
WS_PRIVATE_URL=wss://fstream.binance.com/ws
ENABLE_PRIVATE_WEBSOCKET=true
PRIVATE_CHANNELS=order_update,account_update,balance_update
```

#### **Quote Server Configuration** (`cpp/config/quote_server_binance.ini`)
```ini
[GLOBAL]
EXCHANGE_NAME=BINANCE
ASSET_TYPE=futures

[WEBSOCKET]
WS_PUBLIC_URL=wss://fstream.binance.com/ws
WS_RECONNECT_INTERVAL_MS=5000

[MARKET_DATA]
SYMBOLS=BTCUSDT,ETHUSDT,ADAUSDT
COLLECT_TICKER=true
COLLECT_ORDERBOOK=true
```

### **Environment Variables**
| Variable | Description | Required |
|----------|-------------|----------|
| **Python DeFi Variables** | | |
| `ETHEREUM_RPC_URL` | Ethereum RPC endpoint (Infura/Alchemy) | Yes |
| `WALLET_PRIVATE_KEY` | Ethereum wallet private key | Yes |
| `UNISWAP_V3_ROUTER_ADDRESS` | Uniswap V3 router contract address | No |
| **C++ CeFi Variables** | | |
| `BINANCE_API_KEY` | Binance API key | Yes* |
| `BINANCE_API_SECRET` | Binance API secret | Yes* |
| `DERIBIT_API_KEY` | Deribit API key | Yes* |
| `DERIBIT_API_SECRET` | Deribit API secret | Yes* |
| `GRVT_API_KEY` | GRVT API key | Yes* |
| `GRVT_API_SECRET` | GRVT API secret | Yes* |
| **System Variables** | | |
| `LOG_LEVEL` | Log level (DEBUG/INFO/WARN/ERROR) | No |
| `ENVIRONMENT` | Environment (development/staging/production) | No |
| `DATA_DIR` | Data directory path | No |

*At least one exchange API key pair is required

---

## 📊 **Monitoring & Observability**

### **Logs**
```bash
# View Python DeFi LP logs
tail -f logs/python_lp.log
tail -f logs/uniswap_v3_lp.log
tail -f logs/inventory_publisher.log

# View C++ CeFi process logs
tail -f logs/trader.log
tail -f logs/quote_server_binance.log
tail -f logs/trading_engine_binance.log
tail -f logs/position_server_binance.log

# View all logs
tail -f logs/*.log
```

### **Health Checks**
```bash
# Check Python DeFi LP health
ps aux | grep python
pgrep -f "main.py"

# Check C++ process health
ps aux | grep trader
ps aux | grep quote_server
ps aux | grep trading_engine
ps aux | grep position_server

# Check ZMQ connectivity
netstat -tulpn | grep :6000  # Inventory delta
netstat -tulpn | grep :6001  # Market data
netstat -tulpn | grep :6002  # Order events
```

### **Metrics**
The system logs structured metrics for both components:

**Python DeFi LP Metrics**:
- **LP Performance**: Fee collection rates, impermanent loss
- **Inventory Management**: Position tracking, rebalance frequency
- **Model Performance**: GLFT/AS model effectiveness
- **Gas Usage**: Transaction costs and optimization

**C++ CeFi Metrics**:
- **Trader**: Order generation rates, strategy performance
- **Quote Server**: Market data processing rates, WebSocket connection status
- **Trading Engine**: Order execution rates, HTTP/WebSocket connectivity
- **Position Server**: Position update rates, balance tracking
- **System-wide**: ZMQ message rates, process health status

---

## 🔧 **Operations**

### **Starting/Stopping**
```bash
# Start all processes
docker-compose up -d

# Stop all processes gracefully
docker-compose down

# Restart specific process
docker-compose restart trader
docker-compose restart quote_server_binance
docker-compose restart trading_engine_binance
docker-compose restart position_server_binance

# Start specific process only
docker-compose up -d trader
```

### **Updates**
```bash
# Pull latest changes
git pull

# Rebuild and restart all processes
docker-compose down
docker build -t asymmetric_lp_trader .
docker-compose up -d

# Update specific process
docker-compose stop trading_engine_binance
docker-compose up -d trading_engine_binance
```

### **Data Management**
```bash
# Backup configuration and data
tar -czf backup_$(date +%Y%m%d).tar.gz cpp/config/ data/ logs/

# View data files
ls -la data/
# orders.csv, positions.csv, trades.csv, market_data.csv

# View configuration files
ls -la cpp/config/
# trader.ini, quote_server_*.ini, trading_engine_*.ini, position_server_*.ini
```

---

## 🚨 **Troubleshooting**

### **Common Issues**

#### **1. API Authentication Errors**
```bash
# Check API keys for specific exchange
docker-compose logs trading_engine_binance | grep "API error"
docker-compose logs trading_engine_deribit | grep "API error"

# Verify environment variables
docker exec asymmetric_lp_trading_engine_binance env | grep API_KEY
```

#### **2. Process Communication Issues**
```bash
# Check ZMQ connectivity
docker-compose logs trader | grep "ZMQ"
docker-compose logs quote_server_binance | grep "ZMQ"

# Restart specific process
docker-compose restart trading_engine_binance
```

#### **3. WebSocket Connection Issues**
```bash
# Check WebSocket connections
docker-compose logs quote_server_binance | grep "WebSocket"
docker-compose logs trading_engine_binance | grep "WebSocket"

# Check network connectivity
docker exec asymmetric_lp_quote_server_binance ping fstream.binance.com
```

#### **4. High Memory Usage**
```bash
# Check resource usage for all processes
docker stats

# Adjust limits in docker-compose.yml per process
```

### **Debug Mode**
```bash
# Run specific process in debug mode
docker-compose run --rm trader --log-level DEBUG
docker-compose run --rm quote_server_binance --log-level DEBUG
docker-compose run --rm trading_engine_binance --log-level DEBUG
```

---

## 🔒 **Security**

### **API Key Security**
- API keys are loaded from environment variables
- Never commit API keys to version control
- Use `.env` file for local development
- Rotate API keys regularly

### **Network Security**
- Container runs as non-root user
- Limited network access
- No exposed ports (except monitoring)

### **Data Security**
- Data files are stored in mounted volumes
- Regular backups recommended
- Access logs for audit trails

---

## 📈 **Performance**

### **Resource Requirements**
- **CPU**: 2-4 cores recommended (per process scaling)
- **Memory**: 2-4GB RAM (distributed across processes)
- **Storage**: 20GB for logs, data, and configurations
- **Network**: Low latency connection to exchanges
- **Processes**: 4+ processes per exchange (trader + quote_server + trading_engine + position_server)

### **Optimization**
- Use SSD storage for data directory
- Ensure low-latency network connection
- Monitor memory usage and adjust limits
- Use production-grade hardware

---

## 🆘 **Support**

### **Logs Location**
- Container logs: `docker-compose logs [process_name]`
- File logs: `logs/[process_name].log`
- Data files: `data/` directory
- Configuration files: `cpp/config/` directory

### **Emergency Procedures**
1. **Stop All Trading**: `docker-compose down`
2. **Stop Specific Exchange**: `docker-compose stop trading_engine_[exchange]`
3. **Check Positions**: Review `data/positions.csv`
4. **Manual Orders**: Use exchange web interface
5. **Restart System**: `docker-compose up -d`
6. **Restart Specific Process**: `docker-compose restart [process_name]`

---

## 🎯 **Next Steps**

### **COMPLETED FEATURES**
1. ✅ **Multi-Process Architecture** - Complete per-process system
2. ✅ **Per-Process Configuration** - Individual config files
3. ✅ **Dual Connectivity** - HTTP + WebSocket for each exchange
4. ✅ **Exchange Integration** - Binance, Deribit, GRVT support
5. ✅ **ZMQ Communication** - High-performance inter-process messaging
6. ✅ **Process Management** - Individual process control and monitoring

### **FUTURE ENHANCEMENTS**
1. **Advanced Analytics Dashboard** - Real-time monitoring UI
2. **Machine Learning Integration** - AI-powered strategy optimization
3. **Multi-Asset Support** - Additional trading pairs
4. **Advanced Order Types** - Complex order strategies
5. **Compliance Features** - Regulatory reporting and audit trails

---

**🎉 The multi-process trading system is now production-ready with comprehensive per-process configuration, dual connectivity (HTTP + WebSocket), robust inter-process communication, and complete exchange integration!**
