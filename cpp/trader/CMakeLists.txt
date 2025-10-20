find_package(simdjson REQUIRED)

# Trader Library (core trading logic)
add_library(trader_lib STATIC
    trader_lib.cpp
    zmq_oms_adapter.cpp
    strategy_container.cpp
    mini_oms.cpp
    mini_pms.cpp
)

target_include_directories(trader_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
    ${CMAKE_CURRENT_SOURCE_DIR}/../strategies/base_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../strategies/mm_strategy
    ${ZeroMQ_INCLUDE_DIRS}
)

target_link_libraries(trader_lib PUBLIC
    utils
    exchanges
    base_strategy
    simdjson::simdjson
    proto_msgs
)

# Trader Executable (uses the library)
add_executable(trader
    trader_main.cpp
)

target_include_directories(trader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../exchanges
    ${CMAKE_CURRENT_SOURCE_DIR}/../strategies/base_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../strategies/mm_strategy
    ${ZeroMQ_INCLUDE_DIRS}
)

target_link_libraries(trader
    trader_lib
    utils
    exchanges
    base_strategy
    market_making_strategy
    proto_msgs
)

# Set executable properties
set_target_properties(trader PROPERTIES
    OUTPUT_NAME "trader"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install library and executable
install(TARGETS trader_lib trader
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
