# Find libwebsockets and simdjson
find_package(PkgConfig)
pkg_check_modules(LIBWEBSOCKETS libwebsockets)
find_package(simdjson REQUIRED)
find_package(jsoncpp REQUIRED)
pkg_check_modules(CURL libcurl)

add_library(utils STATIC
  zmq/zmq_publisher.cpp
  zmq/zmq_subscriber.cpp
  mds/orderbook_binary.cpp
  mds/market_data_normalizer.cpp
  mds/parser_factory.cpp
  oms/order_binary.cpp
  # Exchange-specific components moved to exchanges/ folder
  # oms/exchange_monitor.cpp  # Temporarily disabled due to atomic copy issues
  pms/position_binary.cpp
  handlers/message_handler.cpp
  handlers/message_handler_manager.cpp
  config/config_manager.cpp
  config/process_config_manager.cpp
  exchange/exchange_symbol_registry.cpp
  logging/logger.cpp
  app_service/app_service.cpp
  # persistence/database.cpp  # Removed - using exchange-specific data fetchers
)

# Public include dirs so subprojects can include headers
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/oms ${CMAKE_CURRENT_LIST_DIR}/mds ${CMAKE_CURRENT_LIST_DIR}/zmq ${CMAKE_CURRENT_LIST_DIR}/config ${CMAKE_CURRENT_LIST_DIR}/pms ${CMAKE_CURRENT_LIST_DIR}/handlers ${CMAKE_CURRENT_LIST_DIR}/app_service ${CMAKE_CURRENT_LIST_DIR}/exchange ${ZeroMQ_INCLUDE_DIRS})

if(LIBWEBSOCKETS_FOUND)
  target_include_directories(utils PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
  if(CURL_FOUND)
    target_compile_definitions(utils PRIVATE CURL_FOUND)
    target_link_libraries(utils PUBLIC ${ZeroMQ_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto ${LIBWEBSOCKETS_LIBRARIES} simdjson::simdjson jsoncpp_lib ${CURL_LIBRARIES})
  else()
    target_link_libraries(utils PUBLIC ${ZeroMQ_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto ${LIBWEBSOCKETS_LIBRARIES} simdjson::simdjson jsoncpp_lib)
  endif()
else()
  message(WARNING "libwebsockets not found via pkg-config; please install libwebsockets-dev")
  if(CURL_FOUND)
    target_compile_definitions(utils PRIVATE CURL_FOUND)
    target_link_libraries(utils PUBLIC ${ZeroMQ_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto simdjson::simdjson jsoncpp_lib ${CURL_LIBRARIES})
  else()
    target_link_libraries(utils PUBLIC ${ZeroMQ_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto simdjson::simdjson jsoncpp_lib)
  endif()
endif()
