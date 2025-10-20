#pragma once
#include "i_exchange_subscriber.hpp"
#include "binance/public_websocket/binance_subscriber.hpp"
#include "grvt/public_websocket/grvt_subscriber.hpp"
#include "deribit/public_websocket/deribit_subscriber.hpp"
#include <memory>
#include <string>

/**
 * SubscriberFactory - Factory for creating exchange-specific Market Data Subscriber instances
 * 
 * Purpose: Create IExchangeSubscriber implementations at runtime based on exchange name
 * Used by: Market Server processes
 * 
 * Design:
 * - Factory pattern for runtime exchange selection
 * - Returns IExchangeSubscriber interface (not concrete implementations)
 * - Handles exchange-specific configuration internally
 */
class SubscriberFactory {
public:
    // Create subscriber instance for specific exchange
    static std::unique_ptr<IExchangeSubscriber> create_subscriber(const std::string& exchange_name);
    
    // Alias for compatibility
    static std::unique_ptr<IExchangeSubscriber> create(const std::string& exchange_name) {
        return create_subscriber(exchange_name);
    }

private:
    // Helper to convert exchange name to lowercase
    static std::string to_lowercase(const std::string& str);
};
