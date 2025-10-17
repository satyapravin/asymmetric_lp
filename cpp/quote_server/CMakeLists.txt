add_library(exch_common STATIC
  exchanges/common/exchange_manager_base.cpp
  exchanges/common/exchange_manager_base.hpp
)

add_library(exch_binance MODULE
  exchanges/binance/binance_manager.cpp
  exchanges/binance/binance_manager.hpp
  exchanges/binance/binance_lws_client.c
  exchanges/binance/binance_lws_client.h
)
find_package(PkgConfig)
pkg_check_modules(LIBWEBSOCKETS libwebsockets)
find_package(simdjson REQUIRED)
if(LIBWEBSOCKETS_FOUND)
  target_include_directories(exch_binance PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
  target_link_libraries(exch_binance PUBLIC ${LIBWEBSOCKETS_LIBRARIES})
else()
  message(WARNING "libwebsockets not found via pkg-config; please install libwebsockets-dev")
endif()
set_target_properties(exch_binance PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
  PREFIX "lib"
)
target_link_libraries(exch_binance PRIVATE exch_common utils OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB websockets dl simdjson::simdjson)
if(TARGET uSockets)
  target_link_libraries(exch_binance PUBLIC uSockets)
elseif(TARGET usockets)
  target_link_libraries(exch_binance PUBLIC usockets)
endif()

add_library(exch_deribit MODULE
  exchanges/deribit/deribit_manager.cpp
  exchanges/deribit/deribit_manager.hpp
  exchanges/deribit/deribit_lws_client.c
  exchanges/deribit/deribit_lws_client.h
)
find_package(PkgConfig)
pkg_check_modules(LIBWEBSOCKETS libwebsockets)
find_package(simdjson REQUIRED)
if(LIBWEBSOCKETS_FOUND)
  target_include_directories(exch_deribit PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
  target_link_libraries(exch_deribit PUBLIC ${LIBWEBSOCKETS_LIBRARIES})
else()
  message(WARNING "libwebsockets not found via pkg-config; please install libwebsockets-dev")
endif()
set_target_properties(exch_deribit PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
  PREFIX "lib"
)
target_link_libraries(exch_deribit PRIVATE exch_common utils OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB websockets dl simdjson::simdjson)
if(TARGET uSockets)
  target_link_libraries(exch_deribit PUBLIC uSockets)
elseif(TARGET usockets)
  target_link_libraries(exch_deribit PUBLIC usockets)
endif()

add_executable(quote_server
  quote_server_main.cpp
  quote_server.cpp
  exchange_manager.cpp
  exchange_manager_factory.cpp
)

target_link_libraries(quote_server PRIVATE utils exch_common dl)

target_include_directories(quote_server PRIVATE ${ZeroMQ_INCLUDE_DIRS})
