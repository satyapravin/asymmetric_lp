#!/bin/bash

# Asymmetric LP Trading System - Dependency Checker
# This script checks if all required dependencies are installed

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
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Function to check if a package is installed
check_package() {
    local package="$1"
    if dpkg -l | grep -q "^ii  $package "; then
        return 0
    else
        return 1
    fi
}

# Function to check if a library exists
check_library() {
    local library="$1"
    if ldconfig -p | grep -q "$library"; then
        return 0
    else
        return 1
    fi
}

# Function to check if a header exists
check_header() {
    local header="$1"
    if find /usr/include -name "$header" 2>/dev/null | grep -q .; then
        return 0
    else
        return 1
    fi
}

# Function to check if a command exists
check_command() {
    local command="$1"
    if command -v "$command" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Function to check uWebSockets (removed - using only libuv)
check_uwebsockets() {
    return 0  # Always return success since uWebSockets is no longer needed
}

# Main dependency checking function
check_dependencies() {
    local all_good=true
    
    print_status "Checking Asymmetric LP Trading System dependencies..."
    echo ""
    
    # System packages
    print_status "System Packages:"
    local packages=(
        "build-essential:Essential build tools (gcc, g++, make)"
        "cmake:CMake build system"
        "git:Git version control"
        "pkg-config:Package configuration tool"
        "libzmq3-dev:ZeroMQ messaging library"
        "libwebsockets-dev:WebSocket server/client library"
        "libssl-dev:OpenSSL cryptographic library"
        "libcurl4-openssl-dev:HTTP client library"
        "libjsoncpp-dev:JSON parsing library"
        "libsimdjson-dev:High-performance JSON parser"
        "libprotobuf-dev:Protocol Buffers library"
        "protobuf-compiler:Protocol Buffers compiler"
        "libuv1-dev:Asynchronous I/O library"
        "python3:Python interpreter"
        "python3-pip:Python package manager"
        "python3-venv:Python virtual environment"
        "python3-dev:Python development headers"
    )
    
    for package_info in "${packages[@]}"; do
        local package=$(echo "$package_info" | cut -d: -f1)
        local description=$(echo "$package_info" | cut -d: -f2)
        
        if check_package "$package"; then
            print_success "$package - $description"
        else
            print_error "$package - $description (MISSING)"
            all_good=false
        fi
    done
    
    echo ""
    
    # Libraries
    print_status "Libraries:"
    local libraries=(
        "libzmq:ZeroMQ library"
        "libwebsockets:WebSocket library"
        "libssl:OpenSSL library"
        "libcurl:libcurl library"
        "libjsoncpp:jsoncpp library"
        "libsimdjson:simdjson library"
        "libprotobuf:Protocol Buffers library"
        "libuv:libuv library"
    )
    
    for lib_info in "${libraries[@]}"; do
        local lib=$(echo "$lib_info" | cut -d: -f1)
        local description=$(echo "$lib_info" | cut -d: -f2)
        
        if check_library "$lib"; then
            print_success "$lib - $description"
        else
            print_error "$lib - $description (MISSING)"
            all_good=false
        fi
    done
    
    echo ""
    
    # Headers
    print_status "Header Files:"
    local headers=(
        "zmq.h:ZeroMQ headers"
        "libwebsockets.h:WebSocket headers"
        "openssl/ssl.h:OpenSSL headers"
        "curl/curl.h:libcurl headers"
        "json/json.h:jsoncpp headers"
        "simdjson.h:simdjson headers"
        "google/protobuf/message.h:Protocol Buffers headers"
        "uv.h:libuv headers"
    )
    
    for header_info in "${headers[@]}"; do
        local header=$(echo "$header_info" | cut -d: -f1)
        local description=$(echo "$header_info" | cut -d: -f2)
        
        if check_header "$header"; then
            print_success "$header - $description"
        else
            print_error "$header - $description (MISSING)"
            all_good=false
        fi
    done
    
    echo ""
    
    # Commands
    print_status "Commands:"
    local commands=(
        "gcc:GNU C compiler"
        "g++:GNU C++ compiler"
        "make:Make build tool"
        "cmake:CMake build system"
        "git:Git version control"
        "pkg-config:Package configuration"
        "protoc:Protocol Buffers compiler"
        "python3:Python interpreter"
        "pip3:Python package installer"
    )
    
    for cmd_info in "${commands[@]}"; do
        local cmd=$(echo "$cmd_info" | cut -d: -f1)
        local description=$(echo "$cmd_info" | cut -d: -f2)
        
        if check_command "$cmd"; then
            print_success "$cmd - $description"
        else
            print_error "$cmd - $description (MISSING)"
            all_good=false
        fi
    done
    
    echo ""
    
    # External dependencies
    print_status "External Dependencies:"
    
    # uWebSockets (removed - using only libuv)
    print_success "uWebSockets - WebSocket server library (removed, using only libuv)"
    
    # doctest (will be downloaded by CMake)
    print_success "doctest - Testing framework (auto-downloaded by CMake)"
    
    echo ""
    
    # Summary
    if [ "$all_good" = true ]; then
        print_success "All dependencies are installed and ready!"
        echo ""
        print_status "You can now build the project with:"
        echo "  make all"
        echo "  or"
        echo "  ./scripts/build.sh"
        return 0
    else
        print_error "Some dependencies are missing!"
        echo ""
        print_status "To install missing dependencies, run:"
        echo "  ./scripts/install-dependencies.sh"
        echo "  or"
        echo "  make install-deps"
        return 1
    fi
}

# Function to show help
show_help() {
    echo "Asymmetric LP Trading System - Dependency Checker"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --help           Show this help message"
    echo "  --quiet          Only show missing dependencies"
    echo ""
    echo "This script checks if all required dependencies are installed."
}

# Main function
main() {
    local quiet_mode=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --help)
                show_help
                exit 0
                ;;
            --quiet)
                quiet_mode=true
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    if [ "$quiet_mode" = true ]; then
        # Quiet mode - only show missing dependencies
        check_dependencies 2>/dev/null || true
    else
        # Normal mode - show full output
        check_dependencies
    fi
}

# Run main function with all arguments
main "$@"
