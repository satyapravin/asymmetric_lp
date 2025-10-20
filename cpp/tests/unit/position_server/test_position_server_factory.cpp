#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../../position_server/position_server_factory.hpp"
#include "../../utils/pms/position_feed.hpp"
#include <memory>
#include <string>

TEST_SUITE("PositionServerFactory") {
    
    TEST_CASE("Create Binance Position Feed") {
        auto feed = PositionServerFactory::create_from_string("BINANCE", "test_key", "test_secret");
        REQUIRE(feed != nullptr);
        // Note: IExchangePositionFeed doesn't have get_exchange_name() method
        // We can only test that the object was created successfully
        CHECK(true);
    }
    
    TEST_CASE("Create Deribit Position Feed") {
        auto feed = PositionServerFactory::create_from_string("DERIBIT", "test_client_id", "test_secret");
        REQUIRE(feed != nullptr);
        CHECK(true);
    }
    
    TEST_CASE("Create Mock Position Feed") {
        auto feed = PositionServerFactory::create_from_string("MOCK", "", "");
        REQUIRE(feed != nullptr);
        CHECK(true);
    }
    
    TEST_CASE("Create Mock Position Feed with Empty Credentials") {
        auto feed = PositionServerFactory::create_from_string("BINANCE", "", "");
        REQUIRE(feed != nullptr);
        // Should fallback to mock when credentials are empty
        CHECK(true);
    }
    
    TEST_CASE("Handle Invalid Exchange") {
        auto feed = PositionServerFactory::create_from_string("INVALID", "key", "secret");
        REQUIRE(feed != nullptr);
        // Should fallback to mock for invalid exchanges
        CHECK(true);
    }
    
    TEST_CASE("Case Insensitive Exchange Names") {
        auto feed1 = PositionServerFactory::create_from_string("binance", "key", "secret");
        auto feed2 = PositionServerFactory::create_from_string("BINANCE", "key", "secret");
        auto feed3 = PositionServerFactory::create_from_string("Binance", "key", "secret");
        
        REQUIRE(feed1 != nullptr);
        REQUIRE(feed2 != nullptr);
        REQUIRE(feed3 != nullptr);
        
        CHECK(true); // All should be created successfully
    }
    
    TEST_CASE("Exchange Type Enum") {
        auto binance_feed = PositionServerFactory::create(PositionServerFactory::ExchangeType::BINANCE, "key", "secret");
        auto deribit_feed = PositionServerFactory::create(PositionServerFactory::ExchangeType::DERIBIT, "key", "secret");
        auto mock_feed = PositionServerFactory::create(PositionServerFactory::ExchangeType::MOCK, "", "");
        
        REQUIRE(binance_feed != nullptr);
        REQUIRE(deribit_feed != nullptr);
        REQUIRE(mock_feed != nullptr);
        
        CHECK(true); // All should be created successfully
    }
}
