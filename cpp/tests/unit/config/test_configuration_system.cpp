#include <doctest.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "../../config/test_config_manager.hpp"
#include "../../../utils/config/config.hpp"
#include "../../../utils/config/config_manager.hpp"

using namespace test_config;

TEST_SUITE("Configuration System Tests") {

    TEST_CASE("Test Configuration Manager - Basic Loading") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Test loading valid configuration
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        // Verify global settings
        CHECK(manager.is_test_mode());
        CHECK_EQ(manager.get_log_level(), "DEBUG");
        CHECK_FALSE(manager.use_mock_exchanges());
    }

    TEST_CASE("Test Configuration Manager - Exchange Config") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        // Test Binance configuration
        auto binance_config = manager.get_exchange_config("BINANCE");
        CHECK_EQ(binance_config.exchange_name, "BINANCE");
        CHECK_FALSE(binance_config.api_key.empty());
        CHECK_FALSE(binance_config.api_secret.empty());
        CHECK_FALSE(binance_config.public_ws_url.empty());
        CHECK_FALSE(binance_config.private_ws_url.empty());
        CHECK_FALSE(binance_config.http_url.empty());
        CHECK(binance_config.testnet);
        CHECK_EQ(binance_config.asset_type, "FUTURES");
        CHECK_EQ(binance_config.symbol, "BTCUSDT");
        CHECK_EQ(binance_config.timeout_ms, 5000);
        CHECK_EQ(binance_config.max_retries, 3);
    }

    TEST_CASE("Test Configuration Manager - Variable Substitution") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Create a temporary config file with variables
        std::ofstream temp_config("temp_test_config.ini");
        temp_config << "[GLOBAL]\n";
        temp_config << "TEST_VAR=test_value\n";
        temp_config << "SUBSTITUTED_VAR=${TEST_VAR}_suffix\n";
        temp_config << "[BINANCE]\n";
        temp_config << "API_KEY=${TEST_VAR}_api_key\n";
        temp_config.close();
        
        CHECK(manager.load_config("temp_test_config.ini"));
        
        auto binance_config = manager.get_exchange_config("BINANCE");
        CHECK_EQ(binance_config.api_key, "test_value_api_key");
        
        // Clean up
        std::remove("temp_test_config.ini");
    }

    TEST_CASE("Test Configuration Manager - Missing File") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Test loading non-existent file
        CHECK_FALSE(manager.load_config("non_existent_config.ini"));
    }

    TEST_CASE("Test Configuration Manager - Invalid Format") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Create a temporary config file with invalid format
        std::ofstream temp_config("invalid_config.ini");
        temp_config << "[GLOBAL\n";  // Missing closing bracket
        temp_config << "INVALID_LINE_WITHOUT_EQUALS\n";
        temp_config.close();
        
        // Should handle invalid format gracefully
        CHECK_FALSE(manager.load_config("invalid_config.ini"));
        
        // Clean up
        std::remove("invalid_config.ini");
    }

    TEST_CASE("Test Configuration Manager - Empty Sections") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Create a temporary config file with empty sections
        std::ofstream temp_config("empty_config.ini");
        temp_config << "[GLOBAL]\n";
        temp_config << "[BINANCE]\n";
        temp_config << "[DERIBIT]\n";
        temp_config.close();
        
        CHECK(manager.load_config("empty_config.ini"));
        
        // Should return default configs for empty sections
        auto binance_config = manager.get_exchange_config("BINANCE");
        CHECK_EQ(binance_config.exchange_name, "BINANCE");
        
        auto deribit_config = manager.get_exchange_config("DERIBIT");
        CHECK_EQ(deribit_config.exchange_name, "DERIBIT");
        
        // Clean up
        std::remove("empty_config.ini");
    }

    TEST_CASE("Test Configuration Manager - Test Scenarios") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        auto scenario_config = manager.get_scenario_config();
        CHECK(scenario_config.valid_credentials_test);
        CHECK(scenario_config.invalid_credentials_test);
        CHECK(scenario_config.empty_credentials_test);
        CHECK(scenario_config.concurrent_auth_test);
        CHECK(scenario_config.rate_limiting_test);
        CHECK(scenario_config.token_expiration_test);
        CHECK(scenario_config.mixed_auth_test);
    }

    TEST_CASE("Test Configuration Manager - Test Data") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        auto data_config = manager.get_data_config();
        
        // Check test symbols
        CHECK_FALSE(data_config.test_symbols.empty());
        CHECK_EQ(data_config.test_symbols[0], "BTCUSDT");
        
        // Check test order sizes
        CHECK_FALSE(data_config.test_order_sizes.empty());
        CHECK_EQ(data_config.test_order_sizes[0], 0.1);
        
        // Check test prices
        CHECK_FALSE(data_config.test_prices.empty());
        CHECK_EQ(data_config.test_prices[0], 50000.0);
        
        // Check test sides
        CHECK_FALSE(data_config.test_sides.empty());
        CHECK_EQ(data_config.test_sides[0], "BUY");
        
        // Check test order types
        CHECK_FALSE(data_config.test_order_types.empty());
        CHECK_EQ(data_config.test_order_types[0], "MARKET");
    }

    TEST_CASE("Test Configuration Manager - Mock Config") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        auto mock_config = manager.get_mock_config();
        CHECK_FALSE(mock_config.use_mock_responses);
        CHECK_EQ(mock_config.mock_delay_ms, 100);
        CHECK_EQ(mock_config.mock_error_rate, 0.1);
        CHECK_EQ(mock_config.mock_fill_rate, 0.8);
    }

    TEST_CASE("Test Configuration Manager - Helper Methods") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        // Test helper methods
        std::string api_key = manager.get_test_api_key("BINANCE");
        std::string api_secret = manager.get_test_api_secret("BINANCE");
        std::string public_ws_url = manager.get_public_ws_url("BINANCE");
        std::string private_ws_url = manager.get_private_ws_url("BINANCE");
        std::string http_url = manager.get_http_url("BINANCE");
        
        CHECK_FALSE(api_key.empty());
        CHECK_FALSE(api_secret.empty());
        CHECK_FALSE(public_ws_url.empty());
        CHECK_FALSE(private_ws_url.empty());
        CHECK_FALSE(http_url.empty());
    }

    TEST_CASE("Test Configuration Manager - Unknown Exchange") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        // Test unknown exchange
        auto unknown_config = manager.get_exchange_config("UNKNOWN_EXCHANGE");
        CHECK_EQ(unknown_config.exchange_name, "UNKNOWN_EXCHANGE");
        CHECK(unknown_config.api_key.empty());
        CHECK(unknown_config.api_secret.empty());
    }

    TEST_CASE("Test Configuration Manager - Singleton Pattern") {
        TestConfigManager& manager1 = TestConfigManager::get_instance();
        TestConfigManager& manager2 = TestConfigManager::get_instance();
        
        // Should return the same instance
        CHECK_EQ(&manager1, &manager2);
    }

    TEST_CASE("Production Config Manager Tests") {
        // Test production config manager if available
        // This would test the actual production config system
        
        // Create a test production config
        std::ofstream prod_config("test_prod_config.ini");
        prod_config << "[GLOBAL]\n";
        prod_config << "PROCESS_NAME=test_process\n";
        prod_config << "LOG_LEVEL=INFO\n";
        prod_config << "[BINANCE]\n";
        prod_config << "API_KEY=prod_api_key\n";
        prod_config << "API_SECRET=prod_api_secret\n";
        prod_config.close();
        
        // Test loading production config
        AppConfig app_config;
        CHECK(load_from_ini("test_prod_config.ini", app_config));
        
        // Clean up
        std::remove("test_prod_config.ini");
    }

    TEST_CASE("Configuration Validation Tests") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Test configuration validation
        CHECK(manager.load_config("cpp/tests/config/test_exchange_config.ini"));
        
        // Validate required fields
        auto binance_config = manager.get_exchange_config("BINANCE");
        CHECK_FALSE(binance_config.api_key.empty());
        CHECK_FALSE(binance_config.api_secret.empty());
        CHECK_FALSE(binance_config.http_url.empty());
        CHECK(binance_config.timeout_ms > 0);
        CHECK(binance_config.max_retries > 0);
    }

    TEST_CASE("Configuration Thread Safety") {
        TestConfigManager& manager = TestConfigManager::get_instance();
        
        // Test concurrent access to configuration
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                auto binance_config = manager.get_exchange_config("BINANCE");
                auto deribit_config = manager.get_exchange_config("DERIBIT");
                auto scenario_config = manager.get_scenario_config();
                auto data_config = manager.get_data_config();
                
                // Should not crash with concurrent access
                CHECK_FALSE(binance_config.exchange_name.empty());
                CHECK_FALSE(deribit_config.exchange_name.empty());
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
}
