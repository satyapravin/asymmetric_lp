# Production Dockerfile for Asymmetric LP Trader
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install system dependencies
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
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -m -s /bin/bash trader
USER trader
WORKDIR /home/trader

# Copy source code
COPY --chown=trader:trader . /home/trader/asymmetric_lp/

# Build the application
WORKDIR /home/trader/asymmetric_lp/cpp/build
RUN cmake .. && make -j$(nproc)

# Create data directory
RUN mkdir -p /home/trader/data

# Set up entrypoint script
COPY --chown=trader:trader docker/entrypoint.sh /home/trader/entrypoint.sh
RUN chmod +x /home/trader/entrypoint.sh

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD pgrep -f "production_trader" || exit 1

# Default command
ENTRYPOINT ["/home/trader/entrypoint.sh"]
CMD ["--config", "/home/trader/config/production.ini"]
