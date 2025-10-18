#!/bin/bash
set -e

# Production entrypoint script for Asymmetric LP Trader

echo "[ENTRYPOINT] Starting Asymmetric LP Trader..."

# Set default environment variables
export ENVIRONMENT=${ENVIRONMENT:-production}
export LOG_LEVEL=${LOG_LEVEL:-INFO}
export DATA_DIR=${DATA_DIR:-/home/trader/data}

# Validate required environment variables
if [ -z "$BINANCE_API_KEY" ] && [ -z "$DERIBIT_API_KEY" ]; then
    echo "[ENTRYPOINT] ERROR: At least one exchange API key must be provided"
    exit 1
fi

# Create necessary directories
mkdir -p "$DATA_DIR"
mkdir -p /home/trader/logs
mkdir -p /home/trader/config

# Copy default config if not provided
if [ ! -f /home/trader/config/production.ini ]; then
    echo "[ENTRYPOINT] Copying default production config..."
    cp /home/trader/asymmetric_lp/cpp/trader/config.production.ini /home/trader/config/production.ini
fi

# Set up signal handling
cleanup() {
    echo "[ENTRYPOINT] Received shutdown signal, cleaning up..."
    # Send SIGTERM to the main process
    if [ ! -z "$MAIN_PID" ]; then
        kill -TERM "$MAIN_PID" 2>/dev/null || true
        wait "$MAIN_PID" 2>/dev/null || true
    fi
    echo "[ENTRYPOINT] Cleanup complete"
    exit 0
}

trap cleanup SIGTERM SIGINT

# Start the main application
echo "[ENTRYPOINT] Starting production trader..."
cd /home/trader/asymmetric_lp/cpp/build

# Run the production trader with all arguments
./trader/production_trader \
    --config /home/trader/config/production.ini \
    --log-file /home/trader/logs/trader.log \
    --log-level "$LOG_LEVEL" \
    --data-dir "$DATA_DIR" \
    "$@" &

MAIN_PID=$!

# Wait for the main process
wait "$MAIN_PID"
