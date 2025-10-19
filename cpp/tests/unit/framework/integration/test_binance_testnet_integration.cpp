#include <doctest/doctest.h>
#include "../../../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../../../exchanges/binance/public_websocket/binance_subscriber.hpp"
#include "../../../exchanges/binance/private_websocket/binance_oms.hpp"
#include "../../../exchanges/config/api_endpoint_config.hpp"
#include "test_config_loader.hpp"
#include <string>
#include <vector>

TEST_CASE("Binance Testnet Integration") {
    test_config::TestConfigLoader config_loader("test_config.ini");
    
    if (!config_loader.has_credentials()) {
        CHECK(true); // Skip test - credentials not available
        return;
    }
    
    std::string api_key = config_loader.get_api_key();
    std::string secret = config_loader.get_secret();
    std::string test_symbol = config_loader.get_test_symbol();
    
    SUBCASE("Initialize data fetcher") {
        binance::BinanceDataFetcher fetcher(api_key, secret);
        
        // Set authentication
        fetcher.set_auth_credentials(api_key, secret);
        CHECK(fetcher.is_authenticated() == true);
    }
    
    SUBCASE("Initialize subscriber") {
        binance::BinanceSubscriberConfig config;
        config.websocket_url = "wss://testnet.binance.vision/ws/";
        config.testnet = true;
        
        binance::BinanceSubscriber subscriber(config);
        CHECK(true); // Basic initialization test
    }
    
    SUBCASE("Initialize OMS") {
        binance::BinanceConfig config;
        config.api_key = api_key;
        config.api_secret = secret;
        config.base_url = "https://testnet.binance.vision";
        config.testnet = true;
        
        binance::BinanceOMS oms(config);
        CHECK(true); // Basic initialization test
    }
}