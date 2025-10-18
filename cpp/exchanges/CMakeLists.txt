# Exchanges shared library
# Contains all exchange-specific handlers, fetchers, and OMS implementations

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(CURL)

# HTTP handlers (shared by all exchanges)
add_library(http_handlers STATIC
    ../utils/http/i_http_handler.hpp
    ../utils/http/curl_http_handler.hpp
    ../utils/http/curl_http_handler.cpp
)

target_include_directories(http_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

if(CURL_FOUND)
    target_link_libraries(http_handlers CURL)
    target_compile_definitions(http_handlers PRIVATE CURL_FOUND)
endif()

# WebSocket handlers (shared by all exchanges)
add_library(websocket_handlers STATIC
    ../utils/websocket/i_websocket_handler.hpp
    ../utils/websocket/libuv_websocket_handler.hpp
)

target_include_directories(websocket_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# API endpoint configuration
add_library(api_endpoint_config STATIC
    config/api_endpoint_config.hpp
    config/api_endpoint_config.cpp
)

target_include_directories(api_endpoint_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

target_link_libraries(api_endpoint_config
    jsoncpp_lib
)

target_include_directories(exchange_handler_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

target_link_libraries(exchange_handler_base
    http_handlers
    websocket_handlers
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
)

# Binance exchange implementation
add_library(binance_exchange STATIC
    binance/binance_oms.hpp
    binance/binance_oms.cpp
    binance/binance_data_fetcher.hpp
    binance/binance_data_fetcher.cpp
    binance/binance_exchange_handler.hpp
    binance/binance_exchange_handler.cpp
)

target_include_directories(binance_exchange PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

target_link_libraries(binance_exchange
    exchange_handler_base
    http_handlers
    websocket_handlers
    api_endpoint_config
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
)

# Main exchanges shared library
add_library(exchanges SHARED
    # Base components
    exchange_handler.hpp
    exchange_handler.cpp
    
    # Binance implementation
    binance/binance_oms.hpp
    binance/binance_oms.cpp
    binance/binance_data_fetcher.hpp
    binance/binance_data_fetcher.cpp
    binance/binance_exchange_handler.hpp
    binance/binance_exchange_handler.cpp
    
    # Future exchanges can be added here:
    # deribit/deribit_oms.hpp
    # deribit/deribit_oms.cpp
    # grvt/grvt_oms.hpp
    # grvt/grvt_oms.cpp
)

target_include_directories(exchanges PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

target_link_libraries(exchanges
    http_handlers
    websocket_handlers
    api_endpoint_config
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
)

# Set library properties
set_target_properties(exchanges PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER "exchange_handler.hpp;binance/binance_oms.hpp;binance/binance_data_fetcher.hpp;binance/binance_exchange_handler.hpp"
)

# Install targets
install(TARGETS exchanges
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/exchanges
)

install(DIRECTORY binance/ DESTINATION include/exchanges/binance
    FILES_MATCHING PATTERN "*.hpp"
)