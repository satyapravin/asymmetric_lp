#pragma once
#include "i_exchange_data_fetcher.hpp"
#include <memory>
#include <string>

namespace exchanges {

/**
 * DataFetcherFactory - Factory for creating exchange-specific Data Fetcher instances
 * 
 * Purpose: Create IExchangeDataFetcher implementations for startup state recovery
 * Used by: Trading Engine during startup to query open orders
 * 
 * Design:
 * - Factory pattern for runtime exchange selection
 * - Returns IExchangeDataFetcher interface (not concrete implementations)
 * - Handles exchange-specific configuration internally
 */
class DataFetcherFactory {
public:
    /**
     * Create a DataFetcher instance for the specified exchange
     * @param exchange_name The name of the exchange (e.g., "binance", "deribit", "grvt")
     * @param api_key API key for authentication
     * @param api_secret API secret for authentication
     * @return A unique pointer to the DataFetcher implementation, or nullptr if not supported
     */
    static std::unique_ptr<IExchangeDataFetcher> create(const std::string& exchange_name,
                                                        const std::string& api_key,
                                                        const std::string& api_secret);
    
    /**
     * Check if an exchange is supported
     * @param exchange_name The name of the exchange
     * @return true if the exchange is supported, false otherwise
     */
    static bool is_supported(const std::string& exchange_name);

private:
    // Helper function to normalize exchange name
    static std::string normalize_exchange_name(const std::string& exchange_name);
};

} // namespace exchanges

