#!/bin/bash

# Asymmetric LP Trading System - Dependency Installation Script
# This script installs all required third-party dependencies for Ubuntu/Debian systems

set -e

echo "🚀 Installing dependencies for Asymmetric LP Trading System..."

# Update package list
echo "📦 Updating package list..."
sudo apt-get update

# Install system dependencies
echo "🔧 Installing system dependencies..."
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

echo "✅ System dependencies installed successfully!"

# Download uWebSockets if not present
UWS_DIR="/home/pravin/dev/asymmetric_lp/cpp/uWebSockets"
if [ ! -d "$UWS_DIR" ]; then
    echo "📥 Downloading uWebSockets..."
    cd /home/pravin/dev/asymmetric_lp/cpp
    git clone https://github.com/uNetworking/uWebSockets.git uWebSockets
    echo "✅ uWebSockets downloaded successfully!"
else
    echo "✅ uWebSockets already present at $UWS_DIR"
fi

# Verify uWebSockets structure
if [ -f "$UWS_DIR/src/App.h" ]; then
    echo "✅ uWebSockets structure verified"
else
    echo "❌ Error: uWebSockets structure invalid. Expected src/App.h not found."
    exit 1
fi

# Set environment variable for uWebSockets
export UWS_ROOT="$UWS_DIR"
echo "🔧 Set UWS_ROOT=$UWS_ROOT"

# Create build directory and test build
echo "🔨 Testing build configuration..."
cd /home/pravin/dev/asymmetric_lp/cpp
mkdir -p build
cd build

# Test CMake configuration
if cmake -DUWS_ROOT="$UWS_DIR" ..; then
    echo "✅ CMake configuration successful!"
else
    echo "❌ CMake configuration failed!"
    exit 1
fi

echo ""
echo "🎉 All dependencies installed successfully!"
echo ""
echo "📋 Summary of installed dependencies:"
echo "   • System packages: 12 packages"
echo "   • uWebSockets: Downloaded from GitHub"
echo "   • doctest: Will be downloaded by CMake"
echo ""
echo "🚀 You can now build the project with:"
echo "   cd /home/pravin/dev/asymmetric_lp/cpp/build"
echo "   make -j\$(nproc)"
echo ""
echo "🧪 Run tests with:"
echo "   ./tests/run_tests"
echo ""
