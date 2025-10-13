cmake_minimum_required(VERSION 3.15)
project(asymmetric_cpp)

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

add_subdirectory(utils)
add_subdirectory(trader)
add_subdirectory(quote_server)
add_subdirectory(exch_handler)
add_subdirectory(position_server)


