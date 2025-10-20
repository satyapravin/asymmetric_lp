# Trading Engine CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Trading Engine Library (includes both lib and service)
add_library(trading_engine_lib STATIC
    trading_engine_lib.cpp
    trading_engine_service.cpp
)

target_include_directories(trading_engine_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
    ${CMAKE_CURRENT_SOURCE_DIR}/../strategies/base_strategy
)

target_link_libraries(trading_engine_lib PUBLIC
    exchanges
    utils
    proto_msgs
)

# Trading Engine Executable (uses the library)
add_executable(trading_engine
    trading_engine_main.cpp
)

target_include_directories(trading_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
)

target_link_libraries(trading_engine
    trading_engine_lib
    exchanges
    utils
    proto_msgs
)

# Set executable properties
set_target_properties(trading_engine PROPERTIES
    OUTPUT_NAME "trading_engine"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install library and executable
install(TARGETS trading_engine_lib trading_engine
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Create symlinks for each exchange
install(CODE "
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        trading_engine ${CMAKE_INSTALL_PREFIX}/bin/trading_engine_binance)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        trading_engine ${CMAKE_INSTALL_PREFIX}/bin/trading_engine_deribit)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        trading_engine ${CMAKE_INSTALL_PREFIX}/bin/trading_engine_grvt)
")

# Create systemd service files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/trading_engine.service.in
    ${CMAKE_BINARY_DIR}/trading_engine.service
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/trading_engine.service
    DESTINATION /etc/systemd/system
)
