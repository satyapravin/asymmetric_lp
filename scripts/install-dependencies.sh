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

# uWebSockets dependency removed - using only libuv
echo "✅ uWebSockets dependency removed - using only libuv for WebSocket functionality"

# Create build directory and test build
echo "🔨 Testing build configuration..."
cd /home/pravin/dev/asymmetric_lp/cpp
mkdir -p build
cd build

# Test CMake configuration
if cmake ..; then
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
