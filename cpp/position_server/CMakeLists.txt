add_executable(position_server
  position_server_main.cpp
)

target_link_libraries(position_server PRIVATE utils)

target_include_directories(position_server PRIVATE ${ZeroMQ_INCLUDE_DIRS})
