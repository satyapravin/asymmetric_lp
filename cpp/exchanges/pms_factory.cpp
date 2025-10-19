#include "pms_factory.hpp"
#include <algorithm>
#include <iostream>

std::unique_ptr<IExchangePMS> PMSFactory::create_pms(const std::string& exchange_name) {
    std::string exchange = to_lowercase(exchange_name);
    
    if (exchange == "binance") {
        binance::BinancePMSConfig config;
        config.api_key = "";  // Will be set by set_auth_credentials
        config.api_secret = "";
        config.websocket_url = "wss://fstream.binance.com/ws";
        config.testnet = false;
        config.asset_type = "futures";
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<binance::BinancePMS>(config);
    }
    else if (exchange == "grvt") {
        grvt::GrvtPMSConfig config;
        config.api_key = "";  // Will be set by set_auth_credentials
        config.session_cookie = "";
        config.account_id = "";
        config.websocket_url = "wss://api.grvt.io/ws";
        config.testnet = false;
        config.use_lite_version = false;
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<grvt::GrvtPMS>(config);
    }
    else if (exchange == "deribit") {
        deribit::DeribitPMSConfig config;
        config.client_id = "";  // Will be set by set_auth_credentials
        config.client_secret = "";
        config.websocket_url = "wss://www.deribit.com/ws/api/v2";
        config.testnet = true;
        config.currency = "BTC";
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<deribit::DeribitPMS>(config);
    }
    else {
        std::cerr << "[PMS_FACTORY] Unknown exchange: " << exchange_name 
                  << ", using Binance as default" << std::endl;
        
        binance::BinancePMSConfig config;
        config.api_key = "";
        config.api_secret = "";
        config.websocket_url = "wss://fstream.binance.com/ws";
        config.testnet = false;
        config.asset_type = "futures";
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<binance::BinancePMS>(config);
    }
}

std::string PMSFactory::to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}
