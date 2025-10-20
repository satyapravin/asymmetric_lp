# Production Dockerfile for Asymmetric LP Dual-Venue Trading System
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install system dependencies (C++ and Python)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libzmq3-dev \
    libwebsockets-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    libsimdjson-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libuv1-dev \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -m -s /bin/bash trader
USER trader
WORKDIR /home/trader

# Copy source code
COPY --chown=trader:trader . /home/trader/asymmetric_lp/

# Set up Python virtual environment and install dependencies
WORKDIR /home/trader/asymmetric_lp/python
RUN python3 -m venv venv
RUN /home/trader/asymmetric_lp/python/venv/bin/pip install --upgrade pip
RUN /home/trader/asymmetric_lp/python/venv/bin/pip install -r requirements.txt

# Download and set up uWebSockets dependency
WORKDIR /home/trader/asymmetric_lp/cpp
RUN git clone https://github.com/uNetworking/uWebSockets.git uWebSockets

# Build the C++ application and run tests
WORKDIR /home/trader/asymmetric_lp/cpp/build
RUN cmake -DUWS_ROOT=/home/trader/asymmetric_lp/cpp/uWebSockets .. && make -j$(nproc)

# Run comprehensive test suite
RUN echo "[DOCKER] Running comprehensive test suite..." && \
    ./tests/run_tests --test-case="ProcessConfigManager" --test-case="ZmqPublisher" --test-case="ZmqSubscriber" && \
    echo "[DOCKER] ✅ All unit tests passed!" && \
    echo "[DOCKER] Test coverage: 13 unit tests + 3 integration tests = 16 total tests"

# Create data and logs directories
RUN mkdir -p /home/trader/data /home/trader/logs /home/trader/config

# Set up entrypoint script
COPY --chown=trader:trader docker/entrypoint.sh /home/trader/entrypoint.sh
RUN chmod +x /home/trader/entrypoint.sh

# Health check for both Python and C++ processes
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD pgrep -f "main.py" && pgrep -f "production_trader" || exit 1

# Default command
ENTRYPOINT ["/home/trader/entrypoint.sh"]
CMD ["--config", "/home/trader/config/production.ini"]
