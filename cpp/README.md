# C++ Components Build Guide

This project builds `quote_server`, `market_maker`, `exec_handler`, and `position_server`.

## Prerequisites

System packages (Ubuntu):
- build-essential, cmake
- OpenSSL dev: libssl-dev
- Zlib dev: zlib1g-dev
- ZeroMQ dev (optional): libzmq3-dev
- Protocol Buffers (optional): matching protoc and libprotobuf

External source deps (no FetchContent):
- uWebSockets (headers only) cloned locally. Set UWS_ROOT to this clone.
  - Expected header: ${UWS_ROOT}/src/App.h
- libwebsockets installed system-wide (pkg-config provides libwebsockets)
  - Build from source if apt install fails:
    - git clone --depth 1 https://github.com/warmcat/libwebsockets.git
    - cd libwebsockets && mkdir build && cd build
    - cmake .. -DCMAKE_BUILD_TYPE=Release -DLWS_WITH_SHARED=ON -DLWS_WITH_ZIP_FOPS=OFF
    - make -j && sudo make install && sudo ldconfig

## Configure & Build

From repo root:
- rm -rf cpp/build
- cmake -S cpp -B cpp/build -DUWS_ROOT=/absolute/path/to/uWebSockets
- cmake --build cpp/build -j

Notes:
- UWS_ROOT is required and must point to your uWebSockets clone.
- quote_server links libwebsockets and uses a libuv-compatible event loop.
- If Protobuf is available, components build with PROTO_ENABLED automatically.

## Run quote_server

- ./cpp/build/quote_server -c /path/to/config.ini

Example config (Binance):

EXCHANGES=BINANCE
MD_PUB_ENDPOINT=tcp://127.0.0.1:6001
PUBLISH_RATE_HZ=20
MAX_DEPTH=50
PARSER=BINANCE

[BINANCE]
SYMBOL=ethusdt
SYMBOL=btcusdt
CHANNEL=depth20@100ms
CHANNEL=trade
PLUGIN_PATH=/absolute/path/to/cpp/build/plugins/libexch_binance.so

## Run other processes

- Exec handler:
  - ./cpp/build/exec_handler
- Position server:
  - ./cpp/build/position_server
- Market maker:
  - ./cpp/build/market_maker

Ensure topics/endpoints in your ini align across processes (MD_PUB_ENDPOINT, ORD topics, POS endpoints).

## Troubleshooting
- If CMake errors with "UWS_ROOT is not set" or cannot find src/App.h, verify clone path and -DUWS_ROOT.
- If libwebsockets not found, ensure it was installed to /usr/local and sudo ldconfig ran.
- If ZeroMQ not found, install: sudo apt-get install -y libzmq3-dev
