find_package(simdjson REQUIRED)

# Create static library for position server components
add_library(position_server_lib STATIC
  position_server_factory.cpp
  exchanges/binance/binance_position_feed.cpp
  exchanges/binance/binance_lws_client.c
  exchanges/deribit/deribit_position_feed.cpp
  exchanges/deribit/deribit_lws_client.c
)

target_link_libraries(position_server_lib PRIVATE utils simdjson::simdjson)

target_include_directories(position_server_lib PRIVATE 
  ${ZeroMQ_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/exchanges/binance
  ${CMAKE_CURRENT_SOURCE_DIR}/exchanges/deribit
)

# Create executable
add_executable(position_server
  position_server_main.cpp
)

target_link_libraries(position_server PRIVATE position_server_lib)
