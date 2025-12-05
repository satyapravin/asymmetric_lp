#include "doctest.h"
#include "../../../exchanges/grvt/private_websocket/grvt_oms.hpp"
#include "../../../exchanges/grvt/grvt_auth.hpp"
#include "../../../proto/order.pb.h"
#include <memory>

TEST_CASE("GrvtOMS - Initialization") {
    // Test initialization without credentials (should not be authenticated)
    grvt::GrvtOMSConfig config_empty;
    grvt::GrvtOMS oms_empty(config_empty);
    CHECK(oms_empty.is_authenticated() == false);  // Not authenticated without credentials
    
    // Test initialization with credentials (should be authenticated)
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    
    // Test that OMS is initialized and authenticated when credentials are provided
    CHECK(oms.is_authenticated() == true);  // Authenticated when all credentials are provided
    CHECK_NOTHROW(oms.is_connected());  // Should not crash
}

TEST_CASE("GrvtOMS - Authentication") {
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    
    // OMS should be authenticated if all credentials are provided in config
    CHECK(oms.is_authenticated() == true);
    
    // Test authentication with session cookie (backward compatibility)
    // Note: set_auth_credentials requires account_id to be set separately for backward compatibility
    grvt::GrvtOMSConfig config2;
    config2.api_key = "test_key";
    config2.account_id = "test_account_id";  // Set account_id for backward compatibility
    grvt::GrvtOMS oms2(config2);
    oms2.set_auth_credentials("test_key", "gravity=test_cookie");
    // With all three (api_key, session_cookie, account_id), authentication should succeed
    CHECK(oms2.is_authenticated() == true);
    
    // Test authentication without session cookie
    // This will attempt API key auth which will fail without real credentials
    grvt::GrvtOMS oms3(config2);
    oms3.set_auth_credentials("test_key", "");
    // Authentication will fail without real API key, but should not crash
    CHECK_NOTHROW(oms3.is_authenticated());
    CHECK(oms3.is_authenticated() == false);
}

TEST_CASE("GrvtOMS - Place Limit Order") {
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    oms.set_auth_credentials("test_key", "gravity=test_cookie");
    
    // Test placing limit order (may fail without connection, but should not crash)
    CHECK_NOTHROW(oms.place_limit_order("ETH_USDT_Perp", "BUY", 1.5, 2530.0));
}

TEST_CASE("GrvtOMS - Place Market Order") {
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    oms.set_auth_credentials("test_key", "gravity=test_cookie");
    
    // Test placing market order (may fail without connection, but should not crash)
    CHECK_NOTHROW(oms.place_market_order("ETH_USDT_Perp", "BUY", 1.5));
}

TEST_CASE("GrvtOMS - Cancel Order") {
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    oms.set_auth_credentials("test_key", "gravity=test_cookie");
    
    // Test cancel order (may fail without connection, but should not crash)
    CHECK_NOTHROW(oms.cancel_order("TEST_ORDER_1", "order_12345"));
}

TEST_CASE("GrvtOMS - Connection State") {
    grvt::GrvtOMSConfig config;
    config.api_key = "test_key";
    config.session_cookie = "gravity=test_cookie";
    config.account_id = "test_account_id";
    
    grvt::GrvtOMS oms(config);
    
    // Initially not connected
    CHECK(oms.is_connected() == false);
    
    // Test connection (may fail without real WebSocket, but should not crash)
    CHECK_NOTHROW(oms.connect());
}

