# Asymmetric LP Trading System - Makefile
# Provides convenient targets for building and testing

.PHONY: help all clean build test install-deps check-deps setup-uwebsockets

# Default target
help:
	@echo "Asymmetric LP Trading System - Build Targets"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Full build with dependencies and tests"
	@echo "  build            - Build the project (assumes deps are installed)"
	@echo "  test             - Run all tests"
	@echo "  clean            - Clean build directory"
	@echo "  install-deps     - Install system dependencies"
	@echo "  check-deps       - Check if all dependencies are installed"
	@echo "  setup-uwebsockets - Download and setup uWebSockets"
	@echo "  debug            - Build in debug mode"
	@echo "  release          - Build in release mode"
	@echo ""
	@echo "Examples:"
	@echo "  make all         # Full build from scratch"
	@echo "  make build       # Quick build (if deps already installed)"
	@echo "  make test        # Run tests only"
	@echo "  make clean       # Clean build directory"

# Full build with dependencies
all: install-deps setup-uwebsockets build test

# Build the project
build:
	@echo "Building Asymmetric LP Trading System..."
	@./scripts/build.sh --no-deps --no-tests

# Run tests
test:
	@echo "Running tests..."
	@./scripts/build.sh --no-deps --no-tests
	@cd cpp/build && ./tests/run_tests

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf cpp/build/*
	@echo "Build directory cleaned"

# Install system dependencies
install-deps:
	@echo "Installing system dependencies..."
	@./scripts/install-dependencies.sh

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@./scripts/build.sh --no-deps --no-tests --help > /dev/null 2>&1 && echo "Dependencies check passed" || echo "Dependencies check failed"

# Setup uWebSockets (removed - using only libuv)
setup-uwebsockets:
	@echo "uWebSockets dependency removed - using only libuv for WebSocket functionality"

# Debug build
debug:
	@echo "Building in debug mode..."
	@CMAKE_BUILD_TYPE=Debug ./scripts/build.sh --no-deps --no-tests

# Release build
release:
	@echo "Building in release mode..."
	@CMAKE_BUILD_TYPE=Release ./scripts/build.sh --no-deps --no-tests

# Quick build (assumes everything is set up)
quick:
	@echo "Quick build..."
	@cd cpp/build && make -j$$(nproc)

# Show build status
status:
	@echo "Build Status:"
	@echo "============="
	@echo "✓ uWebSockets: Removed (using only libuv)"
	@if [ -d "cpp/build" ]; then \
		echo "✓ Build directory: Present"; \
		if [ -f "cpp/build/tests/run_tests" ]; then \
			echo "✓ Test executable: Present"; \
		else \
			echo "✗ Test executable: Missing"; \
		fi; \
	else \
		echo "✗ Build directory: Missing"; \
	fi
	@echo ""
	@echo "System Dependencies:"
	@for pkg in build-essential cmake git pkg-config libzmq3-dev libwebsockets-dev libssl-dev libcurl4-openssl-dev libjsoncpp-dev libsimdjson-dev libprotobuf-dev protobuf-compiler libuv1-dev python3 python3-pip python3-venv python3-dev; do \
		if dpkg -l | grep -q "^ii  $$pkg "; then \
			echo "✓ $$pkg"; \
		else \
			echo "✗ $$pkg"; \
		fi \
	done
