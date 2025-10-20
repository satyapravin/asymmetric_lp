# Build System Documentation

This document describes the comprehensive build system for the Asymmetric LP Trading System, including dependency management, build scripts, and testing.

## 🚀 Quick Start

### Option 1: Using Make (Recommended)
```bash
# Full build with dependencies and tests
make all

# Quick build (if dependencies already installed)
make build

# Check build status
make status
```

### Option 2: Using Build Script
```bash
# Full build with dependencies and tests
./scripts/build.sh

# Build without tests
./scripts/build.sh --no-tests

# Clean build from scratch
./scripts/build.sh --clean
```

### Option 3: Manual Steps
```bash
# Install dependencies
./scripts/install-dependencies.sh

# Build project
cd cpp/build
cmake -DUWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets ..
make -j$(nproc)

# Run tests
./tests/run_tests
```

## 📋 Build System Components

### 1. Makefile (`/Makefile`)
Provides convenient targets for common build operations:

- `make all` - Full build with dependencies and tests
- `make build` - Build the project (assumes deps installed)
- `make test` - Run all tests
- `make clean` - Clean build directory
- `make install-deps` - Install system dependencies
- `make check-deps` - Check if dependencies are installed
- `make setup-uwebsockets` - Download and setup uWebSockets
- `make debug` - Build in debug mode
- `make release` - Build in release mode
- `make status` - Show build status and dependency check

### 2. Build Script (`scripts/build.sh`)
Comprehensive build script with advanced options:

```bash
./scripts/build.sh [OPTIONS]

Options:
  --clean          Clean build directory before building
  --no-deps        Skip dependency installation
  --no-tests       Skip running tests
  --debug          Build in debug mode
  --help           Show help message
```

### 3. Dependency Installer (`scripts/install-dependencies.sh`)
Automated dependency installation script:

- Installs all required system packages
- Downloads and sets up uWebSockets
- Verifies installation
- Tests build configuration

### 4. Dependency Checker (`scripts/check-dependencies.sh`)
Comprehensive dependency verification:

```bash
./scripts/check-dependencies.sh [OPTIONS]

Options:
  --help           Show help message
  --quiet          Only show missing dependencies
```

## 🔧 Dependencies

### System Packages (13 packages)
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

## 🏗️ Build Process

### 1. Dependency Installation
```bash
# Check current dependencies
./scripts/check-dependencies.sh

# Install missing dependencies
./scripts/install-dependencies.sh
```

### 2. Environment Setup
```bash
# Set environment variables
export UWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets
export CMAKE_BUILD_TYPE=Release  # or Debug
```

### 3. CMake Configuration
```bash
cd cpp/build
cmake -DUWS_ROOT=/home/pravin/dev/asymmetric_lp/cpp/uWebSockets \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..
```

### 4. Build Execution
```bash
# Build with all cores
make -j$(nproc)

# Or using the build script
./scripts/build.sh --no-deps --no-tests
```

### 5. Testing
```bash
# Run all tests
./tests/run_tests

# Run specific test suites
./tests/run_tests --test-case="ProcessConfigManager"
./tests/run_tests --test-case="ZmqPublisher"
./tests/run_tests --test-case="ZmqSubscriber"
```

## 🐳 Docker Build

The Docker build includes all dependencies and builds the complete system:

```bash
# Build Docker image
docker build -t asymmetric-lp .

# Run with test profile
docker-compose --profile testing up test-runner

# Run production services
docker-compose up
```

## 🔍 Troubleshooting

### Common Issues

#### Missing libuv1-dev
**Error**: `libuv_websocket_transport.cpp: fatal error: uv.h: No such file or directory`
**Solution**: 
```bash
sudo apt-get install libuv1-dev
```

#### Missing uWebSockets
**Error**: `UWS_ROOT is not set`
**Solution**:
```bash
cd cpp
git clone https://github.com/uNetworking/uWebSockets.git uWebSockets
export UWS_ROOT=/path/to/uWebSockets
```

#### CMake Configuration Fails
**Error**: `CMake configuration failed`
**Solution**:
```bash
# Check dependencies
./scripts/check-dependencies.sh

# Install missing packages
./scripts/install-dependencies.sh

# Clean and reconfigure
make clean
./scripts/build.sh --clean
```

#### Build Fails
**Error**: `Build failed`
**Solution**:
```bash
# Check for missing dependencies
./scripts/check-dependencies.sh

# Clean build
make clean

# Rebuild with verbose output
./scripts/build.sh --clean --debug
```

### Debug Mode

Build in debug mode for development:

```bash
# Using Makefile
make debug

# Using build script
./scripts/build.sh --debug

# Manual
CMAKE_BUILD_TYPE=Debug ./scripts/build.sh --no-deps --no-tests
```

## 📊 Build Status

Check the current build status:

```bash
make status
```

This will show:
- uWebSockets presence
- Build directory status
- Test executable availability
- System dependency status

## 🔄 Maintenance

### Updating Dependencies
```bash
# Update system packages
sudo apt-get update && sudo apt-get upgrade

# Update uWebSockets
cd cpp/uWebSockets
git pull origin master

# Rebuild project
make clean
make all
```

### Cleaning Build
```bash
# Clean build directory
make clean

# Or using build script
./scripts/build.sh --clean
```

## 📝 Notes

- **libuv1-dev** is the most critical dependency
- **uWebSockets** must be cloned locally
- **doctest** is automatically downloaded by CMake
- All dependencies are included in Docker builds
- Python dependencies are managed via `requirements.txt`
- Build system supports both Debug and Release modes
- Comprehensive error checking and helpful error messages
- Automated dependency verification and installation
