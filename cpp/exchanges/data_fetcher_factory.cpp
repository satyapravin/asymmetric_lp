#include "data_fetcher_factory.hpp"
#include "../utils/logging/log_helper.hpp"
#include <algorithm>

// Include exchange-specific DataFetcher implementations
#include "binance/http/binance_data_fetcher.hpp"
#include "deribit/http/deribit_data_fetcher.hpp"
#include "grvt/http/grvt_data_fetcher.hpp"

namespace exchanges {

std::unique_ptr<IExchangeDataFetcher> DataFetcherFactory::create(
    const std::string& exchange_name,
    const std::string& api_key,
    const std::string& api_secret) {
    
    std::string normalized_name = normalize_exchange_name(exchange_name);
    
    LOG_INFO_COMP("DATA_FETCHER_FACTORY", "Creating DataFetcher for exchange: " + normalized_name);
    
    if (normalized_name == "binance") {
        if (api_key.empty() || api_secret.empty()) {
            LOG_ERROR_COMP("DATA_FETCHER_FACTORY", "Missing required Binance credentials");
            return nullptr;
        }
        return std::make_unique<binance::BinanceDataFetcher>(api_key, api_secret);
    } else if (normalized_name == "deribit") {
        if (api_key.empty() || api_secret.empty()) {
            LOG_ERROR_COMP("DATA_FETCHER_FACTORY", "Missing required Deribit credentials");
            return nullptr;
        }
        return std::make_unique<deribit::DeribitDataFetcher>(api_key, api_secret);
    } else if (normalized_name == "grvt") {
        if (api_key.empty()) {
            LOG_ERROR_COMP("DATA_FETCHER_FACTORY", "Missing required GRVT API key");
            return nullptr;
        }
        // GRVT uses API key only (will authenticate internally)
        return std::make_unique<grvt::GrvtDataFetcher>(api_key);
    } else {
        LOG_ERROR_COMP("DATA_FETCHER_FACTORY", "Unsupported exchange: " + exchange_name);
        return nullptr;
    }
}

bool DataFetcherFactory::is_supported(const std::string& exchange_name) {
    std::string normalized_name = normalize_exchange_name(exchange_name);
    return normalized_name == "binance" || 
           normalized_name == "deribit" || 
           normalized_name == "grvt";
}

std::string DataFetcherFactory::normalize_exchange_name(const std::string& exchange_name) {
    std::string normalized = exchange_name;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Handle common variations
    if (normalized == "binance" || normalized == "binance_futures") {
        return "binance";
    } else if (normalized == "deribit" || normalized == "deribit_futures") {
        return "deribit";
    } else if (normalized == "grvt" || normalized == "grvt_futures") {
        return "grvt";
    }
    
    return normalized;
}

} // namespace exchanges

