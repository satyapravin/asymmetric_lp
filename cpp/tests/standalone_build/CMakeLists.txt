cmake_minimum_required(VERSION 3.16)
project(asymmetric_tests)

# Find required packages
find_package(simdjson REQUIRED)

# Add doctest
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Create a minimal test executable that doesn't depend on the full system
add_executable(run_tests
  basic_tests.cpp
)

# Link with minimal required libraries
target_link_libraries(run_tests PRIVATE 
  simdjson::simdjson
  doctest::doctest
)

# Include directories
target_include_directories(run_tests PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/config
)

# Add test to CTest
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
  TIMEOUT 30
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
