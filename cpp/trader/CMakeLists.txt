find_package(simdjson REQUIRED)

# Create static library for trader components (ZMQ proxies + strategy container + mini OMS)
add_library(trader_lib STATIC
  zmq_oms_adapter.cpp
  strategy_container.cpp
  mini_oms.cpp
)

target_link_libraries(trader_lib PRIVATE utils simdjson::simdjson base_strategy)

target_include_directories(trader_lib PRIVATE 
  ${ZeroMQ_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Single trader executable
add_executable(trader
  trader_main.cpp
)

target_link_libraries(trader PRIVATE utils trader_lib proto_msgs base_strategy)

target_include_directories(trader PRIVATE ${ZeroMQ_INCLUDE_DIRS})
