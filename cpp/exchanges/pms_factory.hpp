#pragma once
#include "i_exchange_pms.hpp"
#include "binance/private_websocket/binance_pms.hpp"
#include "grvt/private_websocket/grvt_pms.hpp"
#include "deribit/private_websocket/deribit_pms.hpp"
#include <memory>
#include <string>

/**
 * PMSFactory - Factory for creating exchange-specific Position Management System instances
 * 
 * Purpose: Create IExchangePMS implementations at runtime based on exchange name
 * Used by: Position Server processes
 * 
 * Design:
 * - Factory pattern for runtime exchange selection
 * - Returns IExchangePMS interface (not concrete implementations)
 * - Handles exchange-specific configuration internally
 */
class PMSFactory {
public:
    // Create PMS instance for specific exchange
    static std::unique_ptr<IExchangePMS> create_pms(const std::string& exchange_name);

private:
    // Helper to convert exchange name to lowercase
    static std::string to_lowercase(const std::string& str);
};
