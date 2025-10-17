#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../utils/oms/exchange_oms_factory.hpp"
#include "../utils/oms/enhanced_oms.hpp"
#include <memory>
#include <string>

TEST_SUITE("ExchangeOMSFactory") {
    
    TEST_CASE("Create Mock Exchange OMS") {
        ExchangeConfig config;
        config.name = "TEST_MOCK";
        config.type = "MOCK";
        config.fill_probability = 0.8;
        config.reject_probability = 0.1;
        config.response_delay_ms = 100;
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_MOCK");
    }
    
    TEST_CASE("Create Binance Exchange OMS") {
        ExchangeConfig config;
        config.name = "TEST_BINANCE";
        config.type = "BINANCE";
        config.api_key = "test_key";
        config.api_secret = "test_secret";
        config.fill_probability = 0.85;
        config.reject_probability = 0.05;
        config.response_delay_ms = 120;
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_BINANCE");
    }
    
    TEST_CASE("Create Deribit Exchange OMS") {
        ExchangeConfig config;
        config.name = "TEST_DERIBIT";
        config.type = "DERIBIT";
        config.api_key = "test_client_id";
        config.api_secret = "test_secret";
        config.fill_probability = 0.75;
        config.reject_probability = 0.10;
        config.response_delay_ms = 180;
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_DERIBIT");
    }
    
    TEST_CASE("Create GRVT Exchange OMS") {
        ExchangeConfig config;
        config.name = "TEST_GRVT";
        config.type = "GRVT";
        config.api_key = "test_key";
        config.api_secret = "test_secret";
        config.fill_probability = 0.90;
        config.reject_probability = 0.03;
        config.response_delay_ms = 80;
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_GRVT");
    }
    
    TEST_CASE("Handle Invalid Exchange Type") {
        ExchangeConfig config;
        config.name = "TEST_INVALID";
        config.type = "INVALID";
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        CHECK(oms == nullptr);
    }
    
    TEST_CASE("Exchange Configuration Defaults") {
        ExchangeConfig config;
        config.name = "TEST_DEFAULTS";
        config.type = "MOCK";
        // No other fields set - should use defaults
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_DEFAULTS");
    }
    
    TEST_CASE("Custom Parameters") {
        ExchangeConfig config;
        config.name = "TEST_CUSTOM";
        config.type = "MOCK";
        config.custom_params["CUSTOM_FIELD"] = "custom_value";
        config.custom_params["RATE_LIMIT"] = "100";
        
        auto oms = ExchangeOMSFactory::create_exchange(config);
        REQUIRE(oms != nullptr);
        CHECK(oms->get_exchange_name() == "TEST_CUSTOM");
    }
    
    TEST_CASE("Get Supported Types") {
        auto types = ExchangeOMSFactory::get_supported_types();
        REQUIRE(types.size() > 0);
        
        // Check that common types are supported
        bool has_mock = std::find(types.begin(), types.end(), "MOCK") != types.end();
        bool has_binance = std::find(types.begin(), types.end(), "BINANCE") != types.end();
        bool has_deribit = std::find(types.begin(), types.end(), "DERIBIT") != types.end();
        
        CHECK(has_mock);
        CHECK(has_binance);
        CHECK(has_deribit);
    }
}
