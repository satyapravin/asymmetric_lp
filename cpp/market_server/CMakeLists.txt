# Market Server CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Market Server executable
add_executable(market_server
    market_server_main.cpp
)

target_include_directories(market_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(market_server PRIVATE
    exchanges
    utils
    proto_msgs
)

# Set executable properties
set_target_properties(market_server PROPERTIES
    OUTPUT_NAME "market_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install executable
install(TARGETS market_server
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
