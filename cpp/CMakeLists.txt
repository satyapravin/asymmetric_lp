cmake_minimum_required(VERSION 3.15)
project(asymmetric_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

add_subdirectory(utils)
add_subdirectory(trader)
add_subdirectory(quote_server)
add_subdirectory(exch_handler)


