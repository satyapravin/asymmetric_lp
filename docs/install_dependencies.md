# Dependency Installation Guide

This guide helps you install all required dependencies for building the Asymmetric LP trading system.

## System Dependencies

### Ubuntu/Debian (Recommended)

```bash
# Update package list
sudo apt-get update

# Install all required dependencies
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

### CentOS/RHEL/Fedora

```bash
# For CentOS/RHEL 8+
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconfig \
    zeromq-devel \
    libwebsockets-devel \
    openssl-devel \
    libcurl-devel \
    jsoncpp-devel \
    simdjson-devel \
    protobuf-devel \
    protobuf-compiler \
    libuv-devel \
    python3 \
    python3-pip \
    python3-venv \
    python3-devel

# For older CentOS/RHEL 7
sudo yum install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconfig \
    zeromq-devel \
    libwebsockets-devel \
    openssl-devel \
    libcurl-devel \
    jsoncpp-devel \
    simdjson-devel \
    protobuf-devel \
    protobuf-compiler \
    libuv-devel \
    python3 \
    python3-pip \
    python3-venv \
    python3-devel
```

### macOS (with Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install \
    cmake \
    pkg-config \
    zeromq \
    libwebsockets \
    openssl \
    curl \
    jsoncpp \
    simdjson \
    protobuf \
    libuv \
    python3
```

## Python Dependencies

After installing system dependencies, install Python packages:

```bash
# Navigate to Python directory
cd python

# Create virtual environment (recommended)
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install Python dependencies
pip install -r requirements.txt
```

## Verification

### Check System Dependencies

```bash
# Check if all required libraries are installed
pkg-config --exists libzmq && echo "✅ ZeroMQ found" || echo "❌ ZeroMQ missing"
pkg-config --exists libwebsockets && echo "✅ libwebsockets found" || echo "❌ libwebsockets missing"
pkg-config --exists libcurl && echo "✅ libcurl found" || echo "❌ libcurl missing"
pkg-config --exists jsoncpp && echo "✅ jsoncpp found" || echo "❌ jsoncpp missing"

# Check simdjson
pkg-config --exists simdjson && echo "✅ simdjson found" || echo "❌ simdjson missing"

# Check protobuf
protoc --version && echo "✅ protobuf found" || echo "❌ protobuf missing"
```

### Build Test

```bash
# Navigate to C++ directory
cd cpp

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)  # On macOS: make -j$(sysctl -n hw.ncpu)
```

## Troubleshooting

### Common Issues

1. **"Could not find simdjson"**
   ```bash
   # Install simdjson development package
   sudo apt-get install libsimdjson-dev  # Ubuntu/Debian
   sudo dnf install simdjson-devel        # CentOS/RHEL/Fedora
   brew install simdjson                  # macOS
   ```

2. **"Could not find libwebsockets"**
   ```bash
   # Install libwebsockets development package
   sudo apt-get install libwebsockets-dev  # Ubuntu/Debian
   sudo dnf install libwebsockets-devel    # CentOS/RHEL/Fedora
   brew install libwebsockets              # macOS
   ```

3. **"Could not find ZeroMQ"**
   ```bash
   # Install ZeroMQ development package
   sudo apt-get install libzmq3-dev  # Ubuntu/Debian
   sudo dnf install zeromq-devel     # CentOS/RHEL/Fedora
   brew install zeromq               # macOS
   ```

4. **"Could not find protobuf"**
   ```bash
   # Install protobuf development package
   sudo apt-get install libprotobuf-dev protobuf-compiler  # Ubuntu/Debian
   sudo dnf install protobuf-devel protobuf-compiler        # CentOS/RHEL/Fedora
   brew install protobuf                                    # macOS
   ```

### Manual Installation (if package manager fails)

If your system doesn't have the required packages, you can build them from source:

#### Building Protobuf from Source

```bash
# Install dependencies for building protobuf
sudo apt-get install -y autoconf automake libtool curl make g++ unzip

# Download and build protobuf
wget https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-3.21.12.tar.gz
tar -xzf protobuf-cpp-3.21.12.tar.gz
cd protobuf-3.21.12

# Configure and build
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install

# Update library cache
sudo ldconfig

# Verify installation
protoc --version
```

#### Building simdjson from Source

```bash
# Example: Building simdjson from source
git clone https://github.com/simdjson/simdjson.git
cd simdjson
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

#### Building ZeroMQ from Source

```bash
# Install dependencies
sudo apt-get install -y libtool pkg-config build-essential autoconf automake

# Download and build ZeroMQ
wget https://github.com/zeromq/libzmq/releases/download/v4.3.4/zeromq-4.3.4.tar.gz
tar -xzf zeromq-4.3.4.tar.gz
cd zeromq-4.3.4
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

## Support

If you continue to have issues:

1. Check your system's package manager documentation
2. Ensure you have the development headers installed (usually `-dev` or `-devel` packages)
3. Verify CMake can find the packages with `pkg-config --list-all`
4. Check the build logs for specific missing dependencies
