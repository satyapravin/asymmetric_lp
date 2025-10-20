cmake_minimum_required(VERSION 3.15)
project(asymmetric_cpp)

# Require UWS_ROOT to be set and valid
set(UWS_ROOT "" CACHE PATH "Path to local uWebSockets clone (required)")
if(NOT UWS_ROOT)
  message(FATAL_ERROR "UWS_ROOT is not set. Please set -DUWS_ROOT=/absolute/path/to/uWebSockets (contains src/App.h)")
endif()
set(UWS_DIR ${UWS_ROOT})

# uWebSockets headers live under src (App.h)
if(EXISTS ${UWS_DIR}/src/App.h)
  message(STATUS "uWebSockets headers at: ${UWS_DIR}/src")
  include_directories(${UWS_DIR}/src)
  if(EXISTS ${UWS_DIR}/uSockets/src)
    include_directories(${UWS_DIR}/uSockets/src)
  endif()
else()
  message(FATAL_ERROR "uWebSockets headers not found under ${UWS_DIR}/src. Ensure UWS_ROOT points to the uWebSockets clone (with src/App.h present)")
endif()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Allow environment overrides for Protobuf locations
if(DEFINED ENV{PROTOBUF_ROOT})
  list(PREPEND CMAKE_PREFIX_PATH $ENV{PROTOBUF_ROOT})
endif()
if(DEFINED ENV{PROTOC})
  set(Protobuf_PROTOC_EXECUTABLE $ENV{PROTOC} CACHE FILEPATH "Protoc executable" FORCE)
elseif(DEFINED ENV{PROTOBUF_PROTOC})
  set(Protobuf_PROTOC_EXECUTABLE $ENV{PROTOBUF_PROTOC} CACHE FILEPATH "Protoc executable" FORCE)
endif()
if(DEFINED ENV{PROTOBUF_INCLUDE_DIR})
  set(Protobuf_INCLUDE_DIRS $ENV{PROTOBUF_INCLUDE_DIR} CACHE PATH "Protobuf include" FORCE)
endif()
if(DEFINED ENV{PROTOBUF_LIBRARY})
  set(Protobuf_LIBRARIES $ENV{PROTOBUF_LIBRARY} CACHE FILEPATH "Protobuf lib" FORCE)
elseif(DEFINED ENV{PROTOBUF_LIB_DIR})
  find_library(PROTOBUF_LIB protobuf PATHS $ENV{PROTOBUF_LIB_DIR} NO_DEFAULT_PATH)
  if(PROTOBUF_LIB)
    set(Protobuf_LIBRARIES ${PROTOBUF_LIB} CACHE FILEPATH "Protobuf lib" FORCE)
  endif()
endif()

# Protobuf generation (optional; uses env overrides if provided)
find_package(Protobuf QUIET)

# If headers/libs set via ENV but package not found, mark as found
if(NOT Protobuf_FOUND AND Protobuf_INCLUDE_DIRS AND Protobuf_LIBRARIES)
  set(Protobuf_FOUND ON)
  if(NOT Protobuf_VERSION AND DEFINED ENV{PROTOBUF_VERSION})
    set(Protobuf_VERSION $ENV{PROTOBUF_VERSION})
  endif()
endif()

if(Protobuf_FOUND AND Protobuf_PROTOC_EXECUTABLE)
  # Verify protoc version matches library version; skip if mismatch
  execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE} --version OUTPUT_VARIABLE PROTOC_VER_RAW OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(REGEX REPLACE ".*libprotoc ([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" PROTOC_VER "${PROTOC_VER_RAW}")
  if(Protobuf_VERSION AND NOT "${PROTOC_VER}" STREQUAL "${Protobuf_VERSION}")
    message(WARNING "Protobuf version mismatch: protoc=${PROTOC_VER}, headers=${Protobuf_VERSION}. Skipping proto codegen.")
    set(Protobuf_FOUND OFF CACHE BOOL "" FORCE)
  endif()
endif()

if(Protobuf_FOUND)
  message(STATUS "Protobuf found: ${Protobuf_VERSION}")
  include_directories(${Protobuf_INCLUDE_DIRS})
  file(GLOB PROTO_FILES ${CMAKE_SOURCE_DIR}/proto/*.proto)
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
  add_library(proto_msgs STATIC ${PROTO_SRCS} ${PROTO_HDRS})
  target_compile_options(proto_msgs PRIVATE -fPIC)
  target_include_directories(proto_msgs PUBLIC ${CMAKE_BINARY_DIR} ${Protobuf_INCLUDE_DIRS})
  target_link_libraries(proto_msgs PUBLIC ${Protobuf_LIBRARIES})
  add_definitions(-DPROTO_ENABLED)
else()
  message(WARNING "Protobuf not found or mismatched - skipping proto codegen. Use env PROTOC/PROTOBUF_INCLUDE_DIR/PROTOBUF_LIBRARY or PROTOBUF_ROOT to configure.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find ZeroMQ using different methods
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(ZeroMQ libzmq)
endif()

# Fallback to manual detection
if(NOT ZeroMQ_FOUND)
  find_library(ZMQ_LIBRARY zmq)
  find_path(ZMQ_INCLUDE_DIR zmq.h)
  if(ZMQ_LIBRARY AND ZMQ_INCLUDE_DIR)
    set(ZeroMQ_LIBRARIES ${ZMQ_LIBRARY})
    set(ZeroMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR})
    set(ZeroMQ_FOUND TRUE)
  endif()
endif()

# If still not found, create a mock for compilation
if(NOT ZeroMQ_FOUND)
  message(WARNING "ZeroMQ not found - creating mock build")
  set(ZeroMQ_LIBRARIES "")
  set(ZeroMQ_INCLUDE_DIRS "")
  add_definitions(-DMOCK_ZMQ)
endif()

# Build core libraries first
add_subdirectory(utils)
add_subdirectory(exchanges)
add_subdirectory(strategies)

# Build server libraries
add_subdirectory(trading_engine)
add_subdirectory(market_server)
add_subdirectory(position_server)

# Build trader (uses server libraries)
add_subdirectory(trader)

# Build tests (uses all libraries)
add_subdirectory(tests)

# Add test executables
find_package(simdjson REQUIRED)

# Link protobuf to all components that need it
if(Protobuf_FOUND)
  # Compile definitions
  target_compile_definitions(trading_engine_lib PRIVATE PROTO_ENABLED)
  target_compile_definitions(market_server_lib PRIVATE PROTO_ENABLED)
  target_compile_definitions(position_server_lib PRIVATE PROTO_ENABLED)
  target_compile_definitions(trading_engine PRIVATE PROTO_ENABLED)
  target_compile_definitions(market_server PRIVATE PROTO_ENABLED)
  target_compile_definitions(position_server PRIVATE PROTO_ENABLED)
  target_compile_definitions(trader_lib PRIVATE PROTO_ENABLED)
  target_compile_definitions(trader PRIVATE PROTO_ENABLED)
endif()



