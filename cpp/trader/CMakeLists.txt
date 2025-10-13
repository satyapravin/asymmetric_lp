add_executable(market_maker
  main.cpp
  zmq_oms.cpp
  market_making_strategy.cpp
)

target_link_libraries(market_maker PRIVATE utils)

target_include_directories(market_maker PRIVATE ${ZeroMQ_INCLUDE_DIRS})
