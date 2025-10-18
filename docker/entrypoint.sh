#!/bin/bash
set -e

# Production entrypoint script for Asymmetric LP Dual-Venue Trading System

echo "[ENTRYPOINT] Starting Asymmetric LP Dual-Venue Trading System..."

# Set default environment variables
export ENVIRONMENT=${ENVIRONMENT:-production}
export LOG_LEVEL=${LOG_LEVEL:-INFO}
export DATA_DIR=${DATA_DIR:-/home/trader/data}

# Validate required environment variables
if [ -z "$ETHEREUM_RPC_URL" ] && [ -z "$BINANCE_API_KEY" ]; then
    echo "[ENTRYPOINT] ERROR: At least one venue (DeFi or CeFi) must be configured"
    exit 1
fi

# Create necessary directories
mkdir -p "$DATA_DIR"
mkdir -p /home/trader/logs
mkdir -p /home/trader/config

# Copy default configs if not provided
if [ ! -f /home/trader/config/production.ini ]; then
    echo "[ENTRYPOINT] Copying default production config..."
    cp /home/trader/asymmetric_lp/cpp/trader/config.production.ini /home/trader/config/production.ini
fi

# Set up signal handling
cleanup() {
    echo "[ENTRYPOINT] Received shutdown signal, cleaning up..."
    
    # Send SIGTERM to Python process
    if [ ! -z "$PYTHON_PID" ]; then
        kill -TERM "$PYTHON_PID" 2>/dev/null || true
        wait "$PYTHON_PID" 2>/dev/null || true
    fi
    
    # Send SIGTERM to C++ process
    if [ ! -z "$CPP_PID" ]; then
        kill -TERM "$CPP_PID" 2>/dev/null || true
        wait "$CPP_PID" 2>/dev/null || true
    fi
    
    echo "[ENTRYPOINT] Cleanup complete"
    exit 0
}

trap cleanup SIGTERM SIGINT

# Start Python DeFi LP Component (if configured)
if [ ! -z "$ETHEREUM_RPC_URL" ] && [ ! -z "$WALLET_PRIVATE_KEY" ]; then
    echo "[ENTRYPOINT] Starting Python DeFi LP Component..."
    cd /home/trader/asymmetric_lp/python
    
    # Activate virtual environment and start Python
    source venv/bin/activate
    python3 main.py \
        --config config.py \
        --log-level "$LOG_LEVEL" \
        --data-dir "$DATA_DIR" &
    
    PYTHON_PID=$!
    echo "[ENTRYPOINT] Python DeFi LP started with PID: $PYTHON_PID"
else
    echo "[ENTRYPOINT] Skipping Python DeFi LP (not configured)"
fi

# Start C++ CeFi Multi-Process Component (if configured)
if [ ! -z "$BINANCE_API_KEY" ] || [ ! -z "$DERIBIT_API_KEY" ] || [ ! -z "$GRVT_API_KEY" ]; then
    echo "[ENTRYPOINT] Starting C++ CeFi Multi-Process Component..."
    cd /home/trader/asymmetric_lp/cpp/build
    
    # Start the production trader
    ./trader/production_trader \
        --config /home/trader/config/production.ini \
        --log-file /home/trader/logs/trader.log \
        --log-level "$LOG_LEVEL" \
        --data-dir "$DATA_DIR" \
        "$@" &
    
    CPP_PID=$!
    echo "[ENTRYPOINT] C++ CeFi Multi-Process started with PID: $CPP_PID"
else
    echo "[ENTRYPOINT] Skipping C++ CeFi Multi-Process (not configured)"
fi

# Wait for processes
if [ ! -z "$PYTHON_PID" ] && [ ! -z "$CPP_PID" ]; then
    echo "[ENTRYPOINT] Waiting for both Python and C++ processes..."
    wait $PYTHON_PID $CPP_PID
elif [ ! -z "$PYTHON_PID" ]; then
    echo "[ENTRYPOINT] Waiting for Python process..."
    wait $PYTHON_PID
elif [ ! -z "$CPP_PID" ]; then
    echo "[ENTRYPOINT] Waiting for C++ process..."
    wait $CPP_PID
else
    echo "[ENTRYPOINT] ERROR: No components configured to start"
    exit 1
fi
