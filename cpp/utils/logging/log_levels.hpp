#pragma once

/**
 * Log Level Optimization Guidelines
 * 
 * This header documents the log level usage guidelines for the trading system.
 * It helps ensure consistent and appropriate log verbosity across components.
 */

namespace logging {

/**
 * Log Level Usage Guidelines:
 * 
 * ERROR:   Critical errors that require immediate attention
 *          - System failures, crashes, data corruption
 *          - Authentication failures, connection failures
 *          - Order rejections, critical state inconsistencies
 * 
 * WARN:    Warning conditions that may indicate problems
 *          - Degraded performance, high latency
 *          - Retry attempts, fallback mechanisms
 *          - Configuration issues, deprecated features
 *          - Non-critical state inconsistencies
 * 
 * INFO:    Important lifecycle and state change events
 *          - Process startup/shutdown
 *          - Component initialization
 *          - Connection established/lost
 *          - Strategy started/stopped
 *          - Significant state transitions
 *          - Periodic status updates (every 5 minutes)
 * 
 * DEBUG:   Detailed debugging information
 *          - Normal trading flow (orders, fills, positions)
 *          - Message processing details
 *          - State machine transitions
 *          - Detailed function entry/exit
 *          - Only enabled in debug builds or with DEBUG log level
 * 
 * Usage Examples:
 * 
 * LOG_ERROR_COMP("COMPONENT", "Failed to connect to exchange: " + error);
 * LOG_WARN_COMP("COMPONENT", "High latency detected: " + std::to_string(latency_ms) + "ms");
 * LOG_INFO_COMP("COMPONENT", "Component started successfully");
 * LOG_DEBUG_COMP("COMPONENT", "Processing order: " + order_id);
 */

// Log level optimization constants
namespace levels {
    // Components that should log at INFO level for normal operations
    constexpr const char* INFO_LEVEL_COMPONENTS[] = {
        "TRADER_LIB",
        "TRADING_ENGINE",
        "MARKET_SERVER",
        "POSITION_SERVER",
        "STRATEGY_CONTAINER"
    };
    
    // Components that should log at DEBUG level for normal operations
    constexpr const char* DEBUG_LEVEL_COMPONENTS[] = {
        "OMS_ADAPTER",
        "MDS_ADAPTER",
        "PMS_ADAPTER",
        "MINI_OMS",
        "MINI_PMS"
    };
}

} // namespace logging

