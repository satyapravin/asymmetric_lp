add_executable(quote_server
  quote_server_main.cpp
  quote_server.cpp
  exchange_manager.cpp
)

target_link_libraries(quote_server PRIVATE utils)

target_include_directories(quote_server PRIVATE ${ZeroMQ_INCLUDE_DIRS})
