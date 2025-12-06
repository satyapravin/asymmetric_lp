# Strategies Library
# Contains all trading strategy implementations

# Base Strategy (abstract base class)
add_library(base_strategy STATIC
    base_strategy/abstract_strategy.cpp
)

target_include_directories(base_strategy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/proto
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_SOURCE_DIR}/exchanges
)

target_link_libraries(base_strategy
    proto_msgs
    utils
    exchanges
)

# Set compile definitions
target_compile_definitions(base_strategy PRIVATE PROTO_ENABLED)

# Market Making Strategy
add_library(market_making_strategy STATIC
    mm_strategy/market_making_strategy.cpp
    mm_strategy/market_making_strategy_config.cpp
    mm_strategy/models/glft_target.cpp
)

target_include_directories(market_making_strategy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/proto
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_SOURCE_DIR}/exchanges
)

target_link_libraries(market_making_strategy
    base_strategy
    proto_msgs
    utils
    exchanges
)

# Set compile definitions
target_compile_definitions(market_making_strategy PRIVATE PROTO_ENABLED)

# Export the libraries
set(STRATEGIES_LIBRARIES base_strategy market_making_strategy PARENT_SCOPE)
