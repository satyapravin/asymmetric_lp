# Market Server CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Market Server Library (includes both lib and service)
add_library(market_server_lib STATIC
    market_server_service.cpp
    market_server_lib.cpp
)

target_include_directories(market_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(market_server_lib PUBLIC
    exchanges
    utils
    proto_msgs
)

# Market Server Executable (uses the library)
add_executable(market_server
    market_server_main.cpp
)

target_include_directories(market_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(market_server
    market_server_lib
    exchanges
    utils
    proto_msgs
)

# Set executable properties
set_target_properties(market_server PROPERTIES
    OUTPUT_NAME "market_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install library and executable
install(TARGETS market_server_lib market_server
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Create symlinks for each exchange
install(CODE "
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        market_server ${CMAKE_INSTALL_PREFIX}/bin/market_server_binance)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        market_server ${CMAKE_INSTALL_PREFIX}/bin/market_server_deribit)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        market_server ${CMAKE_INSTALL_PREFIX}/bin/market_server_grvt)
")
