#pragma once

/**
 * System-wide constants
 * 
 * Centralized location for magic numbers and configuration constants
 * used throughout the trading system.
 */

namespace constants {

// Timeouts (in milliseconds unless otherwise specified)
namespace timeout {
    constexpr int DEFAULT_WEBSOCKET_MS = 30000;           // 30 seconds
    constexpr int DEFAULT_HTTP_MS = 30000;                // 30 seconds
    constexpr int ORDER_STATE_TIMEOUT_SECONDS = 5;        // 5 seconds
    constexpr int BALANCE_POSITION_TIMEOUT_SECONDS = 10;  // 10 seconds
    constexpr int CONNECTION_TIMEOUT_MS = 10000;          // 10 seconds
    constexpr int READ_TIMEOUT_MS = 30000;                // 30 seconds
}

// Retry configuration
namespace retry {
    constexpr int DEFAULT_MAX_RETRIES = 3;
    constexpr int DEFAULT_RETRY_DELAY_MS = 1000;          // 1 second
    constexpr double EXPONENTIAL_BACKOFF_MULTIPLIER = 2.0;
}

// Polling intervals (in milliseconds)
namespace polling {
    constexpr int OMS_EVENT_POLL_INTERVAL_MS = 50;        // 50ms for responsive event handling
    constexpr int OMS_LOG_INTERVAL = 100;                 // Log every 100 iterations
    constexpr int STATUS_LOG_INTERVAL_SECONDS = 300;      // 5 minutes
}

// ZMQ configuration
namespace zmq {
    constexpr int DEFAULT_HWM = 1000;                     // High water mark
    constexpr int DEFAULT_SNDHWM = 1000;
    constexpr int DEFAULT_RCVHWM = 1000;
}

// Order state machine
namespace order {
    constexpr double FILLED_QTY_EPSILON = 1e-9;          // Floating point comparison epsilon
    constexpr int MAX_ORDER_RETRIES = 3;
    constexpr int ORDER_EXPIRY_SECONDS = 3600;            // 1 hour default expiry
}

// Exchange-specific defaults
namespace exchange {
    namespace binance {
        constexpr const char* DEFAULT_WS_URL = "wss://fstream.binance.com/ws";
        constexpr const char* DEFAULT_HTTP_URL = "https://fapi.binance.com";
        constexpr int DEFAULT_TIMEOUT_MS = 30000;
    }
    
    namespace deribit {
        constexpr const char* DEFAULT_WS_URL = "wss://www.deribit.com/ws/api/v2";
        constexpr const char* DEFAULT_HTTP_URL = "https://www.deribit.com";
        constexpr int DEFAULT_TIMEOUT_MS = 30000;
    }
    
    namespace grvt {
        constexpr const char* DEFAULT_WS_URL = "wss://api.grvt.io/ws";
        constexpr const char* DEFAULT_HTTP_URL = "https://api.grvt.io";
        constexpr int DEFAULT_TIMEOUT_MS = 30000;
    }
}

// Logging intervals
namespace logging {
    constexpr int STATUS_UPDATE_INTERVAL_SECONDS = 300;   // 5 minutes
    constexpr int HEALTH_CHECK_INTERVAL_SECONDS = 60;     // 1 minute
}

} // namespace constants

