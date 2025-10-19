# Position Server CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Position Server executable
add_executable(position_server
    position_server_main.cpp
)

target_include_directories(position_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(position_server PRIVATE
    exchanges
    utils
    proto_msgs
)

# Set executable properties
set_target_properties(position_server PROPERTIES
    OUTPUT_NAME "position_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install executable
install(TARGETS position_server
    RUNTIME DESTINATION bin
)

# Create symlinks for each exchange
install(CODE "
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        position_server ${CMAKE_INSTALL_PREFIX}/bin/position_server_binance)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        position_server ${CMAKE_INSTALL_PREFIX}/bin/position_server_deribit)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        position_server ${CMAKE_INSTALL_PREFIX}/bin/position_server_grvt)
")
