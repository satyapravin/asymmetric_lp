add_library(utils STATIC
  zmq/zmq_publisher.cpp
  zmq/zmq_subscriber.cpp
  mds/orderbook_binary.cpp
  mds/market_data_normalizer.cpp
  mds/parser_factory.cpp
  oms/order_binary.cpp
  pms/position_binary.cpp
  pms/position_feed.cpp
  handlers/message_handler.cpp
  handlers/message_handler_manager.cpp
)

# Public include dirs so subprojects can include headers
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/oms ${CMAKE_CURRENT_LIST_DIR}/mds ${CMAKE_CURRENT_LIST_DIR}/zmq ${CMAKE_CURRENT_LIST_DIR}/config ${CMAKE_CURRENT_LIST_DIR}/pms ${CMAKE_CURRENT_LIST_DIR}/handlers ${ZeroMQ_INCLUDE_DIRS})

target_link_libraries(utils PUBLIC ${ZeroMQ_LIBRARIES})
