#pragma once
#include <memory>
#include <string>
#include "i_exchange_oms.hpp"

namespace exchanges {

/**
 * Factory for creating exchange-specific OMS implementations
 */
class OMSFactory {
public:
    /**
     * Create an OMS implementation for the specified exchange
     * @param exchange_name The name of the exchange (e.g., "binance", "deribit", "grvt")
     * @param config_json Configuration JSON string for the exchange
     * @return A unique pointer to the OMS implementation, or nullptr if not supported
     */
    static std::unique_ptr<IExchangeOMS> create(const std::string& exchange_name, const std::string& config_json);
    
    /**
     * Create an OMS implementation for the specified exchange (simple version)
     * @param exchange_name The name of the exchange (e.g., "binance", "deribit", "grvt")
     * @return A unique pointer to the OMS implementation, or nullptr if not supported
     */
    static std::unique_ptr<IExchangeOMS> create(const std::string& exchange_name);
    
    /**
     * Check if an exchange is supported
     * @param exchange_name The name of the exchange
     * @return true if the exchange is supported, false otherwise
     */
    static bool is_supported(const std::string& exchange_name);
    
    /**
     * Get list of supported exchanges
     * @return Vector of supported exchange names
     */
    static std::vector<std::string> get_supported_exchanges();

private:
    // Helper function to normalize exchange name
    static std::string normalize_exchange_name(const std::string& exchange_name);
};

} // namespace exchanges
