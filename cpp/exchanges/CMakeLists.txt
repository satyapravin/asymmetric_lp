# Exchanges shared library
# Contains all exchange-specific handlers, fetchers, and OMS implementations

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(CURL)

# Include proto headers if available
if(TARGET proto_msgs)
    set(PROTO_INCLUDES ${CMAKE_BINARY_DIR})
else()
    set(PROTO_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../proto)
endif()

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

# WebSocket transport abstraction
add_library(websocket_transport STATIC
    websocket/i_websocket_transport.hpp
    websocket/libuv_websocket_transport.hpp
    websocket/libuv_websocket_transport.cpp
    websocket/websocket_transport.hpp
    websocket/websocket_transport.cpp
    websocket/i_exchange_websocket_handler.hpp
)

target_include_directories(websocket_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Set position independent code for static library
set_target_properties(websocket_transport PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link with real libuv and websockets
find_library(LIBUV_LIBRARY uv)
find_library(WEBSOCKETS_LIBRARY websockets)

if(NOT LIBUV_LIBRARY)
    message(FATAL_ERROR "libuv library not found - required for WebSocket transport")
endif()

if(NOT WEBSOCKETS_LIBRARY)
    message(FATAL_ERROR "websockets library not found - required for WebSocket transport")
endif()

target_link_libraries(websocket_transport ${LIBUV_LIBRARY} ${WEBSOCKETS_LIBRARY})

# WebSocket handlers (legacy - can be removed later)
add_library(websocket_handlers STATIC
    ../utils/websocket/i_websocket_handler.hpp
    ../utils/websocket/libuv_websocket_handler.hpp
    ../utils/websocket/libuv_websocket_handler.cpp
)

target_include_directories(websocket_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Main exchanges shared library
add_library(exchanges SHARED
    # Interface headers
    i_exchange_oms.hpp
    i_exchange_pms.hpp
    i_exchange_data_fetcher.hpp
    i_exchange_subscriber.hpp
    
    # Factory implementations
    pms_factory.hpp
    pms_factory.cpp
    subscriber_factory.hpp
    subscriber_factory.cpp
    oms_factory.hpp
    oms_factory.cpp
    data_fetcher_factory.hpp
    data_fetcher_factory.cpp
    
    # Binance implementation
    binance/http/binance_data_fetcher.hpp
    binance/http/binance_data_fetcher.cpp
    binance/public_websocket/binance_public_websocket_handler.hpp
    binance/public_websocket/binance_public_websocket_handler.cpp
    binance/public_websocket/binance_subscriber.hpp
    binance/public_websocket/binance_subscriber.cpp
    binance/private_websocket/binance_private_websocket_handler.hpp
    binance/private_websocket/binance_private_websocket_handler.cpp
    binance/private_websocket/binance_oms.hpp
    binance/private_websocket/binance_oms.cpp
    binance/private_websocket/binance_pms.hpp
    binance/private_websocket/binance_pms.cpp
    
    # GRVT implementation
    grvt/grvt_auth.hpp
    grvt/grvt_auth.cpp
    grvt/http/grvt_data_fetcher.hpp
    grvt/http/grvt_data_fetcher.cpp
    grvt/public_websocket/grvt_public_websocket_handler.hpp
    grvt/public_websocket/grvt_public_websocket_handler.cpp
    grvt/public_websocket/grvt_subscriber.hpp
    grvt/public_websocket/grvt_subscriber.cpp
    grvt/private_websocket/grvt_private_websocket_handler.hpp
    grvt/private_websocket/grvt_private_websocket_handler.cpp
    grvt/private_websocket/grvt_oms.hpp
    grvt/private_websocket/grvt_oms.cpp
    grvt/private_websocket/grvt_pms.hpp
    grvt/private_websocket/grvt_pms.cpp
    
    # Deribit implementation
    deribit/http/deribit_data_fetcher.hpp
    deribit/http/deribit_data_fetcher.cpp
    deribit/public_websocket/deribit_public_websocket_handler.hpp
    deribit/public_websocket/deribit_public_websocket_handler.cpp
    deribit/public_websocket/deribit_subscriber.hpp
    deribit/public_websocket/deribit_subscriber.cpp
    deribit/private_websocket/deribit_private_websocket_handler.hpp
    deribit/private_websocket/deribit_private_websocket_handler.cpp
    deribit/private_websocket/deribit_oms.hpp
    deribit/private_websocket/deribit_oms.cpp
    deribit/private_websocket/deribit_pms.hpp
    deribit/private_websocket/deribit_pms.cpp
)

target_include_directories(exchanges PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${PROTO_INCLUDES}
)

# Add CURL include directories if found
if(CURL_FOUND)
    target_include_directories(exchanges PUBLIC ${CURL_INCLUDE_DIRS})
    target_compile_definitions(exchanges PRIVATE CURL_FOUND)
else()
    # Fallback to manual CURL detection
    set(CURL_INCLUDE_DIR "/home/pravin/anaconda3/include")
    if(EXISTS ${CURL_INCLUDE_DIR})
        target_include_directories(exchanges PUBLIC ${CURL_INCLUDE_DIR})
        target_compile_definitions(exchanges PRIVATE CURL_FOUND)
    endif()
endif()

target_link_libraries(exchanges
    http_handlers
    websocket_transport
    websocket_handlers
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
)

# Link CURL if found
if(CURL_FOUND)
    target_link_libraries(exchanges ${CURL_LIBRARIES})
else()
    # Fallback: link directly to the library
    target_link_libraries(exchanges /usr/lib/x86_64-linux-gnu/libcurl.so.4)
endif()

# Link proto if available
if(TARGET proto_msgs)
    target_link_libraries(exchanges proto_msgs)
endif()

# Set library properties
set_target_properties(exchanges PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Install targets
install(TARGETS exchanges
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/exchanges
)

install(DIRECTORY binance/ DESTINATION include/exchanges/binance
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY grvt/ DESTINATION include/exchanges/grvt
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY deribit/ DESTINATION include/exchanges/deribit
    FILES_MATCHING PATTERN "*.hpp"
)