# Dependencies Installation Guide

This document provides comprehensive instructions for installing all required dependencies for the Asymmetric LP Trading System.

## 🚀 Quick Installation

### For Ubuntu/Debian Systems
```bash
# Run the automated installation script
./scripts/install-dependencies.sh
```

### For Docker
```bash
# Build with all dependencies included
docker build -t asymmetric-lp .
```

## 📋 Complete Dependency List

### System Packages (12 packages)
- **build-essential** - GCC compiler suite
- **cmake** - Build system generator  
- **git** - Version control
- **pkg-config** - Package configuration tool
- **libzmq3-dev** - ZeroMQ messaging library
- **libwebsockets-dev** - WebSocket server/client library
- **libssl-dev** - OpenSSL cryptographic library
- **libcurl4-openssl-dev** - HTTP client library
- **libjsoncpp-dev** - JSON parsing library
- **libsimdjson-dev** - High-performance JSON parser
- **libprotobuf-dev** - Protocol Buffers library
- **protobuf-compiler** - Protocol Buffers compiler
- **libuv1-dev** - Asynchronous I/O library (CRITICAL)

### External Libraries
- **uWebSockets** - WebSocket server library (GitHub clone)
- **doctest** - Testing framework (v2.4.11, auto-downloaded)

### Python Dependencies
- **python3** - Python interpreter
- **python3-pip** - Python package manager
- **python3-venv** - Python virtual environment
- **python3-dev** - Python development headers

## 🔧 Manual Installation

### 1. Install System Packages
```bash
sudo apt-get update
sudo apt-get install -y \
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
    python3-dev
```

### 2. Download uWebSockets
```bash
cd /home/pravin/dev/asymmetric_lp/cpp
git clone https://github.com/uNetworking/uWebSockets.git uWebSockets
```

### 3. Set Environment Variables
```bash
export UWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets
```

### 4. Build the Project
```bash
cd /home/pravin/dev/asymmetric_lp/cpp
mkdir -p build
cd build
cmake -DUWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets ..
make -j$(nproc)
```

## 🐳 Docker Installation

The Dockerfile includes all dependencies and builds the complete system:

```bash
# Build the Docker image
docker build -t asymmetric-lp .

# Run with test profile
docker-compose --profile testing up test-runner

# Run production services
docker-compose up
```

## 🧪 Testing Dependencies

After installation, verify everything works:

```bash
# Run all tests
cd /home/pravin/dev/asymmetric_lp/cpp/build
./tests/run_tests

# Run specific test suites
./tests/run_tests --test-case="ProcessConfigManager"
./tests/run_tests --test-case="ZmqPublisher"
./tests/run_tests --test-case="ZmqSubscriber"
```

## 🚨 Common Issues

### Missing libuv1-dev
**Error**: `libuv_websocket_transport.cpp: fatal error: uv.h: No such file or directory`
**Solution**: Install `libuv1-dev` package

### Missing uWebSockets
**Error**: `UWS_ROOT is not set`
**Solution**: Clone uWebSockets and set UWS_ROOT environment variable

### Missing doctest
**Error**: Build succeeds but tests fail to compile
**Solution**: doctest is auto-downloaded by CMake, ensure internet connection

### CMake Version
**Error**: `CMake 3.15 or higher is required`
**Solution**: Update CMake: `sudo apt-get install cmake`

## 📊 Dependency Verification

Check if all dependencies are properly installed:

```bash
# Check system packages
dpkg -l | grep -E "(libzmq|libwebsockets|libssl|libcurl|libjsoncpp|libsimdjson|libprotobuf|libuv)"

# Check uWebSockets
ls -la /home/pravin/dev/asymmetric_lp/cpp/uWebSockets/src/App.h

# Check Python
python3 --version
pip3 --version

# Test build
cd /home/pravin/dev/asymmetric_lp/cpp/build
cmake -DUWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets ..
make -j$(nproc)
```

## 🔄 Updating Dependencies

To update dependencies:

```bash
# Update system packages
sudo apt-get update && sudo apt-get upgrade

# Update uWebSockets
cd /home/pravin/dev/asymmetric_lp/cpp/uWebSockets
git pull origin master

# Rebuild project
cd /home/pravin/dev/asymmetric_lp/cpp/build
make clean
make -j$(nproc)
```

## 📝 Notes

- **libuv1-dev** is the most critical missing dependency
- **uWebSockets** must be cloned locally (not available as system package)
- **doctest** is automatically downloaded by CMake FetchContent
- All dependencies are included in the Docker image
- Python dependencies are managed via `requirements.txt`
