cmake_minimum_required(VERSION 3.15)
project(asymmetric_cpp)

# uWebSockets dependency removed - using only libuv for WebSocket functionality

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

# AddressSanitizer support
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O1")
    
    if(ENABLE_ASAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    endif()
    
    if(ENABLE_UBSAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
    
    if(ENABLE_TSAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
        message(STATUS "ThreadSanitizer enabled")
    endif()
    
    # Suppress some common false positives
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer")
endif()

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

# Add top-level test target that runs all tests directly
add_custom_target(test_all_target
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/run_tests
    DEPENDS run_tests integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
    COMMENT "Running all tests (unit + integration)"
)

# Add sanitized build targets
add_custom_target(build_asan
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --parallel
    COMMENT "Building with AddressSanitizer"
)

add_custom_target(build_ubsan
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --parallel
    COMMENT "Building with UndefinedBehaviorSanitizer"
)

add_custom_target(build_tsan
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --parallel
    COMMENT "Building with ThreadSanitizer"
)

# Make test_all_target depend on all libraries being built
add_dependencies(test_all_target
    proto_msgs
    utils
    exchanges
    base_strategy
    trading_engine_lib
    market_server_lib
    position_server_lib
    trader_lib
)

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



