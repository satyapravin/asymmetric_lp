# Position Server CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Position Server Library (includes both lib and service)
add_library(position_server_lib STATIC
    position_server_service.cpp
    position_server_lib.cpp
)

target_include_directories(position_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(position_server_lib PUBLIC
    exchanges
    utils
    proto_msgs
)

# Position Server Executable (uses the library)
add_executable(position_server
    position_server_main.cpp
)

target_include_directories(position_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(position_server
    position_server_lib
    exchanges
    utils
    proto_msgs
)

# Set executable properties
set_target_properties(position_server PROPERTIES
    OUTPUT_NAME "position_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install library and executable
install(TARGETS position_server_lib position_server
    LIBRARY DESTINATION lib
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
