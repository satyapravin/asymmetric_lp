add_executable(quote_server
  quote_server_main.cpp
)

target_link_libraries(quote_server PRIVATE utils)

target_include_directories(quote_server PRIVATE ${ZeroMQ_INCLUDE_DIRS})
