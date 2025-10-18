find_package(simdjson REQUIRED)

# Create static library for trader components
add_library(trader_lib STATIC
  market_making_strategy.cpp
)

target_link_libraries(trader_lib PRIVATE utils simdjson::simdjson)

target_include_directories(trader_lib PRIVATE 
  ${ZeroMQ_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/models
)

add_executable(market_maker
  main.cpp
  zmq_oms.cpp
)

add_executable(trader
  trader_main.cpp
)

add_executable(production_trader
  production_trader_main.cpp
)

target_link_libraries(market_maker PRIVATE utils trader_lib)
target_link_libraries(trader PRIVATE trader_lib)
target_link_libraries(production_trader PRIVATE trader_lib)

target_include_directories(market_maker PRIVATE ${ZeroMQ_INCLUDE_DIRS})
target_include_directories(trader PRIVATE ${ZeroMQ_INCLUDE_DIRS})
target_include_directories(production_trader PRIVATE ${ZeroMQ_INCLUDE_DIRS})
