#include "oms_factory.hpp"
#include <algorithm>
#include <iostream>
#include <json/json.h>
#include <sstream>

// Include exchange-specific OMS implementations
#include "binance/private_websocket/binance_oms.hpp"
#include "deribit/private_websocket/deribit_oms.hpp"
#include "grvt/private_websocket/grvt_oms.hpp"

namespace exchanges {

std::unique_ptr<IExchangeOMS> OMSFactory::create(const std::string& exchange_name, const std::string& config_json) {
    std::string normalized_name = normalize_exchange_name(exchange_name);
    
    std::cout << "[OMS_FACTORY] Creating OMS for exchange: " << normalized_name << std::endl;
    
    // Parse configuration JSON
    Json::Value config;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream config_stream(config_json);
    
    if (!Json::parseFromStream(builder, config_stream, &config, &errors)) {
        std::cerr << "[OMS_FACTORY] Failed to parse configuration JSON: " << errors << std::endl;
        return nullptr;
    }
    
    if (normalized_name == "binance") {
        binance::BinanceConfig binance_config;
        binance_config.api_key = config.get("api_key", "").asString();
        binance_config.api_secret = config.get("api_secret", "").asString();
        binance_config.base_url = config.get("base_url", "").asString();
        binance_config.testnet = config.get("testnet", false).asBool();
        binance_config.max_retries = config.get("max_retries", 3).asInt();
        binance_config.timeout_ms = config.get("timeout_ms", 5000).asInt();
        
        if (binance_config.api_key.empty() || binance_config.api_secret.empty() || binance_config.base_url.empty()) {
            std::cerr << "[OMS_FACTORY] Missing required Binance configuration" << std::endl;
            return nullptr;
        }
        
        return std::make_unique<binance::BinanceOMS>(binance_config);
    } else if (normalized_name == "deribit") {
        deribit::DeribitOMSConfig deribit_config;
        deribit_config.client_id = config.get("client_id", "").asString();
        deribit_config.client_secret = config.get("client_secret", "").asString();
        deribit_config.testnet = config.get("testnet", false).asBool();
        
        if (deribit_config.client_id.empty() || deribit_config.client_secret.empty()) {
            std::cerr << "[OMS_FACTORY] Missing required Deribit configuration" << std::endl;
            return nullptr;
        }
        
        return std::make_unique<deribit::DeribitOMS>(deribit_config);
    } else if (normalized_name == "grvt") {
        grvt::GrvtOMSConfig grvt_config;
        grvt_config.api_key = config.get("api_key", "").asString();
        grvt_config.testnet = config.get("testnet", false).asBool();
        
        if (grvt_config.api_key.empty()) {
            std::cerr << "[OMS_FACTORY] Missing required GRVT configuration" << std::endl;
            return nullptr;
        }
        
        return std::make_unique<grvt::GrvtOMS>(grvt_config);
    } else {
        std::cerr << "[OMS_FACTORY] Unsupported exchange: " << exchange_name << std::endl;
        return nullptr;
    }
}

bool OMSFactory::is_supported(const std::string& exchange_name) {
    std::string normalized_name = normalize_exchange_name(exchange_name);
    
    return normalized_name == "binance" || 
           normalized_name == "deribit" || 
           normalized_name == "grvt";
}

std::vector<std::string> OMSFactory::get_supported_exchanges() {
    return {"binance", "deribit", "grvt"};
}

std::string OMSFactory::normalize_exchange_name(const std::string& exchange_name) {
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