add_executable(grvt_bridge
  main.cpp
  zmq_oms.cpp
  market_making_strategy.cpp
)

target_link_libraries(grvt_bridge PRIVATE utils)

target_include_directories(grvt_bridge PRIVATE ${ZeroMQ_INCLUDE_DIRS})
