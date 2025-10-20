#!/bin/bash

# Asymmetric LP Trading System - Build Script
# This script handles all dependencies and builds the complete system

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a package is installed
check_package() {
    dpkg -l | grep -q "^ii  $1 " && return 0 || return 1
}

# Function to install missing packages
install_packages() {
    local packages=("$@")
    local missing_packages=()
    
    print_status "Checking system dependencies..."
    
    for package in "${packages[@]}"; do
        if check_package "$package"; then
            print_success "$package is installed"
        else
            print_warning "$package is missing"
            missing_packages+=("$package")
        fi
    done
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_status "Installing missing packages: ${missing_packages[*]}"
        sudo apt-get update
        sudo apt-get install -y "${missing_packages[@]}"
        print_success "All packages installed successfully"
    else
        print_success "All required packages are already installed"
    fi
}

# Function to setup uWebSockets (removed - using only libuv)
setup_uwebsockets() {
    print_success "uWebSockets dependency removed - using only libuv for WebSocket functionality"
    return 0
}

# Function to setup build environment
setup_build_env() {
    local build_dir="/home/pravin/dev/asymmetric_lp/cpp/build"
    
    print_status "Setting up build environment..."
    
    # Create build directory
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Set environment variables
    export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
    
    print_success "Build environment configured"
    print_status "Build type: $CMAKE_BUILD_TYPE"
}

# Function to configure CMake
configure_cmake() {
    local build_dir="/home/pravin/dev/asymmetric_lp/cpp/build"
    
    print_status "Configuring CMake..."
    cd "$build_dir"
    
    if cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
             -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
             ..; then
        print_success "CMake configuration successful"
        return 0
    else
        print_error "CMake configuration failed"
        return 1
    fi
}

# Function to build the project
build_project() {
    local build_dir="/home/pravin/dev/asymmetric_lp/cpp/build"
    local num_cores=$(nproc)
    
    print_status "Building project with $num_cores cores..."
    cd "$build_dir"
    
    if make -j"$num_cores"; then
        print_success "Build completed successfully"
        return 0
    else
        print_error "Build failed"
        return 1
    fi
}

# Function to run tests
run_tests() {
    local build_dir="/home/pravin/dev/asymmetric_lp/cpp/build"
    
    print_status "Running tests..."
    cd "$build_dir"
    
    if [ -f "./tests/run_tests" ]; then
        if ./tests/run_tests; then
            print_success "All tests passed"
            return 0
        else
            print_error "Some tests failed"
            return 1
        fi
    else
        print_warning "Test executable not found, skipping tests"
        return 0
    fi
}

# Function to clean build
clean_build() {
    local build_dir="/home/pravin/dev/asymmetric_lp/cpp/build"
    
    print_status "Cleaning build directory..."
    cd "$build_dir"
    make clean
    print_success "Build cleaned"
}

# Function to show help
show_help() {
    echo "Asymmetric LP Trading System - Build Script"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean          Clean build directory before building"
    echo "  --no-deps        Skip dependency installation"
    echo "  --no-tests       Skip running tests"
    echo "  --debug          Build in debug mode"
    echo "  --help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Full build with dependencies and tests"
    echo "  $0 --clean            # Clean build from scratch"
    echo "  $0 --no-tests        # Build without running tests"
    echo "  $0 --debug           # Build in debug mode"
}

# Main function
main() {
    local clean_build_flag=false
    local skip_deps_flag=false
    local skip_tests_flag=false
    local debug_mode=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_build_flag=true
                shift
                ;;
            --no-deps)
                skip_deps_flag=true
                shift
                ;;
            --no-tests)
                skip_tests_flag=true
                shift
                ;;
            --debug)
                debug_mode=true
                CMAKE_BUILD_TYPE="Debug"
                shift
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    print_status "Starting Asymmetric LP Trading System build..."
    
    # Set debug mode if requested
    if [ "$debug_mode" = true ]; then
        CMAKE_BUILD_TYPE="Debug"
        print_status "Building in debug mode"
    fi
    
    # Install dependencies unless skipped
    if [ "$skip_deps_flag" = false ]; then
        local required_packages=(
            "build-essential"
            "cmake"
            "git"
            "pkg-config"
            "libzmq3-dev"
            "libwebsockets-dev"
            "libssl-dev"
            "libcurl4-openssl-dev"
            "libjsoncpp-dev"
            "libsimdjson-dev"
            "libprotobuf-dev"
            "protobuf-compiler"
            "libuv1-dev"
            "python3"
            "python3-pip"
            "python3-venv"
            "python3-dev"
        )
        
        install_packages "${required_packages[@]}"
        setup_uwebsockets
    else
        print_warning "Skipping dependency installation"
    fi
    
    # Setup build environment
    setup_build_env
    
    # Clean build if requested
    if [ "$clean_build_flag" = true ]; then
        clean_build
    fi
    
    # Configure CMake
    if ! configure_cmake; then
        print_error "CMake configuration failed"
        exit 1
    fi
    
    # Build project
    if ! build_project; then
        print_error "Build failed"
        exit 1
    fi
    
    # Run tests unless skipped
    if [ "$skip_tests_flag" = false ]; then
        if ! run_tests; then
            print_error "Tests failed"
            exit 1
        fi
    else
        print_warning "Skipping tests"
    fi
    
    print_success "Build completed successfully!"
    print_status "Executables available in: /home/pravin/dev/asymmetric_lp/cpp/build"
    print_status "Test runner: /home/pravin/dev/asymmetric_lp/cpp/build/tests/run_tests"
}

# Run main function with all arguments
main "$@"
