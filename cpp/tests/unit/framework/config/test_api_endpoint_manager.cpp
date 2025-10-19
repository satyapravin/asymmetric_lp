#include <doctest/doctest.h>
#include "../../../exchanges/config/api_endpoint_config.hpp"
#include <fstream>
#include <sstream>

using namespace exchange_config;

TEST_CASE("ApiEndpointManager - Basic Functionality") {
    ApiEndpointManager manager;
    
    SUBCASE("Load valid configuration") {
        // Create a test config file
        std::string test_config = R"({
            "exchanges": {
                "binance": {
                    "exchange_name": "BINANCE",
                    "version": "v1",
                    "testnet_mode": false,
                    "assets": {
                        "FUTURES": {
                            "type": "FUTURES",
                            "name": "futures",
                            "urls": {
                                "rest_api": "https://fapi.binance.com",
                                "websocket_public": "wss://fstream.binance.com/stream",
                                "websocket_private": "wss://fstream.binance.com/ws"
                            },
                            "endpoints": {
                                "get_open_orders": {
                                    "name": "get_open_orders",
                                    "path": "/fapi/v1/openOrders",
                                    "method": "GET",
                                    "requires_auth": true,
                                    "requires_signature": true,
                                    "description": "Get all open orders"
                                }
                            },
                            "websocket_channels": {
                                "orderbook": "depth",
                                "trades": "trade"
                            }
                        }
                    },
                    "authentication": {
                        "api_key_header": "X-MBX-APIKEY",
                        "signature_param": "signature"
                    }
                }
            }
        })";
        
        std::ofstream config_file("test_config.json");
        config_file << test_config;
        config_file.close();
        
        bool loaded = manager.load_config("test_config.json");
        CHECK(loaded == true);
        
        // Clean up
        std::remove("test_config.json");
    }
    
    SUBCASE("Load invalid configuration") {
        bool loaded = manager.load_config("nonexistent.json");
        CHECK(loaded == false);
    }
}

TEST_CASE("ApiEndpointManager - URL Resolution") {
    ApiEndpointManager manager;
    
    // Load test configuration - following production structure
    std::string test_config = R"({
        "exchanges": {
            "binance": {
                "exchange_name": "BINANCE",
                "assets": {
                    "FUTURES": {
                        "urls": {
                            "rest_api": "https://fapi.binance.com",
                            "websocket_public": "wss://fstream.binance.com/stream",
                            "websocket_private": "wss://fstream.binance.com/ws"
                        },
                        "websocket_channels": {
                            "orderbook": "depth",
                            "trades": "trade"
                        }
                    },
                    "SPOT": {
                        "urls": {
                            "rest_api": "https://api.binance.com",
                            "websocket_public": "wss://stream.binance.com:9443/stream"
                        }
                    }
                },
                "authentication": {
                    "api_key_header": "X-MBX-APIKEY",
                    "signature_param": "signature",
                    "timestamp_param": "timestamp"
                }
            }
        }
    })";
    
    std::ofstream config_file("test_url_config.json");
    config_file << test_config;
    config_file.close();
    
    manager.load_config("test_url_config.json");
    
    SUBCASE("Get REST API URL") {
        std::string futures_url = manager.get_rest_api_url("BINANCE", AssetType::FUTURES);
        CHECK(futures_url == "https://fapi.binance.com");
        
        std::string spot_url = manager.get_rest_api_url("BINANCE", AssetType::SPOT);
        CHECK(spot_url == "https://api.binance.com");
    }
    
    SUBCASE("Get WebSocket URL") {
        std::string public_ws = manager.get_websocket_url("BINANCE", AssetType::FUTURES, "public");
        CHECK(public_ws == "wss://fstream.binance.com/stream");
        
        std::string private_ws = manager.get_websocket_url("BINANCE", AssetType::FUTURES, "private");
        CHECK(private_ws == "wss://fstream.binance.com/ws");
    }
    
    SUBCASE("Get WebSocket Channel Name") {
        std::string orderbook_channel = manager.get_websocket_channel_name("BINANCE", AssetType::FUTURES, "orderbook");
        CHECK(orderbook_channel == "depth");
        
        std::string trades_channel = manager.get_websocket_channel_name("BINANCE", AssetType::FUTURES, "trades");
        CHECK(trades_channel == "trade");
    }
    
    // Clean up
    std::remove("test_url_config.json");
}

TEST_CASE("ApiEndpointManager - Authentication Config") {
    ApiEndpointManager manager;
    
    std::string test_config = R"({
        "exchanges": {
            "binance": {
                "authentication": {
                    "api_key_header": "X-MBX-APIKEY",
                    "signature_param": "signature",
                    "timestamp_param": "timestamp"
                }
            },
            "grvt": {
                "authentication": {
                    "api_key_header": "Authorization",
                    "session_cookie": "session",
                    "account_id_header": "X-Account-ID"
                }
            },
            "deribit": {
                "authentication": {
                    "client_id": "client_id",
                    "client_secret": "client_secret",
                    "grant_type": "client_credentials"
                }
            }
        }
    })";
    
    std::ofstream config_file("test_auth_config.json");
    config_file << test_config;
    config_file.close();
    
    manager.load_config("test_auth_config.json");
    
    SUBCASE("Binance Authentication") {
        AuthConfig binance_auth = manager.get_authentication_config("BINANCE");
        CHECK(binance_auth.api_key_header == "X-MBX-APIKEY");
        CHECK(binance_auth.signature_param == "signature");
        CHECK(binance_auth.timestamp_param == "timestamp");
    }
    
    SUBCASE("GRVT Authentication") {
        AuthConfig grvt_auth = manager.get_authentication_config("GRVT");
        CHECK(grvt_auth.api_key_header == "Authorization");
        CHECK(grvt_auth.session_cookie == "session");
        CHECK(grvt_auth.account_id_header == "X-Account-ID");
    }
    
    SUBCASE("Deribit Authentication") {
        AuthConfig deribit_auth = manager.get_authentication_config("DERIBIT");
        CHECK(deribit_auth.client_id == "client_id");
        CHECK(deribit_auth.client_secret == "client_secret");
        CHECK(deribit_auth.grant_type == "client_credentials");
    }
    
    // Clean up
    std::remove("test_auth_config.json");
}

TEST_CASE("ApiEndpointManager - Error Handling") {
    ApiEndpointManager manager;
    
    SUBCASE("Invalid exchange name") {
        std::string url = manager.get_rest_api_url("INVALID", AssetType::FUTURES);
        CHECK(url.empty());
    }
    
    SUBCASE("Invalid asset type") {
        std::string url = manager.get_rest_api_url("BINANCE", AssetType::OPTIONS);
        CHECK(url.empty());
    }
    
    SUBCASE("Invalid WebSocket type") {
        std::string url = manager.get_websocket_url("BINANCE", AssetType::FUTURES, "invalid");
        CHECK(url.empty());
    }
}

TEST_CASE("ApiEndpointManager - Configuration Validation") {
    ApiEndpointManager manager;
    
    SUBCASE("Valid configuration") {
        std::string valid_config = R"({
            "exchanges": {
                "binance": {
                    "exchange_name": "BINANCE",
                    "assets": {
                        "FUTURES": {
                            "urls": {
                                "rest_api": "https://fapi.binance.com"
                            }
                        }
                    }
                }
            }
        })";
        
        std::ofstream config_file("test_valid_config.json");
        config_file << valid_config;
        config_file.close();
        
        ApiEndpointManager manager;
        bool valid = manager.load_config("test_valid_config.json");
        CHECK(valid == true);
        
        std::remove("test_valid_config.json");
    }
    
    SUBCASE("Invalid JSON") {
        std::string invalid_config = R"({
            "exchanges": {
                "binance": {
                    "exchange_name": "BINANCE",
                    "assets": {
                        "FUTURES": {
                            "urls": {
                                "rest_api": "https://fapi.binance.com"
                            }
                        }
                    }
                }
            }
        )";  // Missing closing brace
        
        std::ofstream config_file("test_invalid_config.json");
        config_file << invalid_config;
        config_file.close();
        
        ApiEndpointManager manager;
        bool valid = manager.load_config("test_invalid_config.json");
        CHECK(valid == false);
        
        std::remove("test_invalid_config.json");
    }
}
