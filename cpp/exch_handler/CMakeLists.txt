add_executable(exec_handler
  execution_handler_main.cpp
)

target_link_libraries(exec_handler PRIVATE utils)

target_include_directories(exec_handler PRIVATE ${ZeroMQ_INCLUDE_DIRS})
