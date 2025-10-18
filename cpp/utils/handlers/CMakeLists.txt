# HTTP handlers library
add_library(http_handlers
    http/i_http_handler.hpp
    http/curl_http_handler.hpp
    http/curl_http_handler.cpp
)

target_include_directories(http_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with CURL if available
find_package(CURL)
if(CURL_FOUND)
    target_link_libraries(http_handlers CURL)
    target_compile_definitions(http_handlers PRIVATE CURL_FOUND)
endif()

# WebSocket handlers library
add_library(websocket_handlers
    websocket/i_websocket_handler.hpp
    websocket/libuv_websocket_handler.hpp
)

target_include_directories(websocket_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with libuv if available
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUV libuv)
    if(LIBUV_FOUND)
        target_link_libraries(websocket_handlers ${LIBUV_LIBRARIES})
        target_include_directories(websocket_handlers PRIVATE ${LIBUV_INCLUDE_DIRS})
        target_compile_definitions(websocket_handlers PRIVATE LIBUV_FOUND)
    endif()
endif()

# Exchange handlers library
add_library(exchange_handlers
    ../exch_handler/exchange_handler.hpp
    ../exch_handler/exchange_handler.cpp
)

target_include_directories(exchange_handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(exchange_handlers
    http_handlers
    websocket_handlers
)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)

target_link_libraries(exchange_handlers
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
)
