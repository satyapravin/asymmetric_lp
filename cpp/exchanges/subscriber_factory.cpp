#include "subscriber_factory.hpp"
#include "../utils/logging/log_helper.hpp"
#include <algorithm>
#include <stdexcept>

std::unique_ptr<IExchangeSubscriber> SubscriberFactory::create_subscriber(const std::string& exchange_name) {
    std::string exchange = to_lowercase(exchange_name);
    
    if (exchange == "binance") {
        binance::BinanceSubscriberConfig config;
        config.websocket_url = "wss://fstream.binance.com/ws";
        config.testnet = false;
        config.asset_type = "futures";
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<binance::BinanceSubscriber>(config);
    }
    else if (exchange == "grvt") {
        grvt::GrvtSubscriberConfig config;
        config.websocket_url = "wss://api.grvt.io/ws";
        config.testnet = false;
        config.use_lite_version = false;
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<grvt::GrvtSubscriber>(config);
    }
    else if (exchange == "deribit") {
        deribit::DeribitSubscriberConfig config;
        config.websocket_url = "wss://www.deribit.com/ws/api/v2";
        config.testnet = true;
        config.currency = "BTC";
        config.timeout_ms = 30000;
        config.max_retries = 3;
        
        return std::make_unique<deribit::DeribitSubscriber>(config);
    }
    else {
        std::string error_msg = "Unknown or unsupported exchange: " + exchange_name;
        LOG_ERROR_COMP("SUBSCRIBER_FACTORY", error_msg);
        throw std::runtime_error(error_msg);
    }
}

std::string SubscriberFactory::to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}
