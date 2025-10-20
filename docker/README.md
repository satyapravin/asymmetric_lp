# Docker Configuration for Asymmetric LP Dual-Venue Trading System

## Overview

This Docker configuration supports the complete dual-venue trading system with both Python DeFi LP and C++ CeFi components running in separate containers with ZMQ communication. The system uses libuv for WebSocket functionality, with uWebSockets dependency removed for simplified architecture.

## Architecture

```
┌─────────────────────┐    ZMQ     ┌─────────────────────┐
│  Python DeFi LP     │ ────────► │  C++ CeFi Multi-    │
│  Container          │   Port    │  Process Container   │
│                     │   6000    │                     │
└─────────────────────┘           └─────────────────────┘
```

## Services

### 1. Python DeFi LP Component (`python-defi`)
- **Purpose**: Uniswap V3 liquidity provision
- **Port**: Publishes on ZMQ port 6000
- **Dependencies**: Ethereum RPC, wallet private key
- **Resource Limits**: 1 CPU, 1GB RAM

### 2. C++ CeFi Multi-Process Component (`cpp-cefi`)
- **Purpose**: Centralized exchange trading
- **Port**: Subscribes to ZMQ port 6000
- **Dependencies**: Exchange API keys (Binance, Deribit, GRVT)
- **Resource Limits**: 2 CPU, 2GB RAM
- **Startup**: Waits for Python DeFi LP to be healthy

### 3. Test Runner Service (`test-runner`)
- **Purpose**: Comprehensive test suite execution
- **Profile**: `testing` (optional service)
- **Dependencies**: Built C++ application and test files
- **Resource Limits**: 1 CPU, 1GB RAM
- **Execution**: Runs all 16 tests (13 unit + 3 integration) with 57 assertions

## Environment Variables

Create a `.env` file with the following variables:

### Python DeFi LP Configuration
```bash
# Ethereum RPC Configuration
ETHEREUM_RPC_URL=https://mainnet.infura.io/v3/YOUR_INFURA_PROJECT_ID
WALLET_PRIVATE_KEY=0xYOUR_PRIVATE_KEY_HERE
UNISWAP_V3_ROUTER_ADDRESS=0xE592427A0AEce92De3Edee1F18E0157C05861564

# ZMQ Configuration
ZMQ_PUBLISHER_HOST=0.0.0.0
ZMQ_PUBLISHER_PORT=6000
```

### C++ CeFi Configuration
```bash
# Exchange API Keys
BINANCE_API_KEY=your_binance_api_key_here
BINANCE_API_SECRET=your_binance_api_secret_here
DERIBIT_API_KEY=your_deribit_api_key_here
DERIBIT_API_SECRET=your_deribit_api_secret_here
GRVT_API_KEY=your_grvt_api_key_here
GRVT_API_SECRET=your_grvt_api_secret_here

# ZMQ Configuration
ZMQ_SUBSCRIBER_HOST=python-defi
ZMQ_SUBSCRIBER_PORT=6000
```

### System Configuration
```bash
ENVIRONMENT=production
LOG_LEVEL=INFO
DATA_DIR=/data
```

## Quick Start

### 1. Create Environment File
```bash
cp docker/.env.template .env
# Edit .env with your actual API keys and configuration
```

### 2. Build and Start Services
```bash
# Build the Docker image
docker-compose build

# Start both services
docker-compose up -d

# View logs
docker-compose logs -f
```

### 3. Start Individual Services
```bash
# Start only Python DeFi LP
docker-compose up python-defi

# Start only C++ CeFi
docker-compose up cpp-cefi

# Run comprehensive test suite
docker-compose --profile testing up test-runner
```

## Monitoring

### Health Checks
Both services include health checks:
- **Python DeFi LP**: Checks for `main.py` process
- **C++ CeFi**: Checks for `production_trader` process

### Optional Monitoring Stack
```bash
# Start with monitoring
docker-compose --profile monitoring up -d

# Access Prometheus: http://localhost:9090
# Access Grafana: http://localhost:3000 (admin/admin)
```

## Data Persistence

### Volumes
- `./data:/data` - Shared data directory
- `./logs:/home/trader/logs` - Log files
- `./python:/home/trader/asymmetric_lp/python` - Python source code
- `./cpp/config:/home/trader/config` - C++ configuration files

### Logs
```bash
# View Python DeFi LP logs
docker-compose logs python-defi

# View C++ CeFi logs
docker-compose logs cpp-cefi

# View all logs
docker-compose logs -f
```

## Troubleshooting

### Common Issues

1. **Python DeFi LP fails to start**
   - Check `ETHEREUM_RPC_URL` and `WALLET_PRIVATE_KEY`
   - Verify Python dependencies in `requirements.txt`

2. **C++ CeFi fails to start**
   - Check exchange API keys
   - Verify C++ build completed successfully

3. **ZMQ communication issues**
   - Ensure both containers are on the same network
   - Check ZMQ port configuration

### Debug Commands
```bash
# Check container status
docker-compose ps

# Check container health
docker-compose ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"

# Access container shell
docker-compose exec python-defi bash
docker-compose exec cpp-cefi bash

# Check logs with timestamps
docker-compose logs -f -t
```

## Security Considerations

1. **API Keys**: Never commit real API keys to version control
2. **Private Keys**: Use Docker secrets for production deployments
3. **Network**: Services communicate over internal Docker network
4. **Permissions**: Run containers as non-root user (`trader`)

## Production Deployment

### Environment-Specific Configuration
```bash
# Production
cp docker/.env.template .env.production
docker-compose -f docker-compose.yml --env-file .env.production up -d

# Staging
cp docker/.env.template .env.staging
docker-compose -f docker-compose.yml --env-file .env.staging up -d
```

### Resource Optimization
- Adjust CPU/memory limits in `docker-compose.yml`
- Use multi-stage builds for smaller images
- Consider using Docker Swarm or Kubernetes for orchestration

## Development

### Local Development
```bash
# Mount source code for live development
docker-compose -f docker-compose.dev.yml up -d
```

### Testing
```bash
# Run tests in containers
docker-compose exec python-defi python -m pytest
docker-compose exec cpp-cefi ./tests/run_tests

# Run specific test suites
docker-compose exec cpp-cefi ./tests/run_tests --test-case="ProcessConfigManager"
docker-compose exec cpp-cefi ./tests/run_tests --test-case="ZmqPublisher"
docker-compose exec cpp-cefi ./tests/run_tests --test-case="ZmqSubscriber"

# Run integration tests
docker-compose exec cpp-cefi ./tests/run_tests --test-case="Full Chain"
docker-compose exec cpp-cefi ./tests/run_tests --test-case="ORDER FLOW"
docker-compose exec cpp-cefi ./tests/run_tests --test-case="POSITION FLOW"

# View test coverage report
docker-compose exec cpp-cefi cat /home/trader/asymmetric_lp/cpp/tests/TEST_COVERAGE_REPORT.md
```

### Test Suite Overview
The Docker build process includes comprehensive test execution:
- **Unit Tests**: 13 tests covering ZMQ communication and configuration management
- **Integration Tests**: 3 tests covering end-to-end data flows
- **Total Coverage**: 16 tests with 57 assertions
- **Build Validation**: Tests run during Docker build to ensure code quality
- **WebSocket Support**: Uses libuv for WebSocket functionality (uWebSockets removed)
