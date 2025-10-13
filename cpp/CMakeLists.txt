cmake_minimum_required(VERSION 3.15)
project(grvt_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

add_executable(grvt_bridge
  src/main.cpp
  src/zmq_subscriber.cpp
  src/glft_target.cpp
  src/grvt_client_stub.cpp
)

target_include_directories(grvt_bridge PRIVATE ${ZeroMQ_INCLUDE_DIRS})
target_link_libraries(grvt_bridge PRIVATE ${ZeroMQ_LIBRARIES})


