#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

namespace test_config {

struct ExchangeTestConfig {
    std::string exchange_name;
    std::string api_key;
    std::string api_secret;
    std::string session_cookie;  // For GRVT
    std::string account_id;       // For GRVT
    std::string public_ws_url;
    std::string private_ws_url;
    std::string http_url;
    bool testnet;
    std::string asset_type;
    std::string symbol;
    int timeout_ms;
    int max_retries;
};

struct TestScenarioConfig {
    bool valid_credentials_test;
    bool invalid_credentials_test;
    bool empty_credentials_test;
    bool concurrent_auth_test;
    bool rate_limiting_test;
    bool token_expiration_test;
    bool mixed_auth_test;
};

struct TestDataConfig {
    std::vector<std::string> test_symbols;
    std::vector<double> test_order_sizes;
    std::vector<double> test_prices;
    std::vector<std::string> test_sides;
    std::vector<std::string> test_order_types;
};

struct MockConfig {
    bool use_mock_responses;
    int mock_delay_ms;
    double mock_error_rate;
    double mock_fill_rate;
};

class TestConfigManager {
public:
    static TestConfigManager& get_instance();
    
    // Load configuration from file
    bool load_config(const std::string& config_file);
    
    // Get exchange configuration
    ExchangeTestConfig get_exchange_config(const std::string& exchange_name) const;
    
    // Get test scenario configuration
    TestScenarioConfig get_scenario_config() const;
    
    // Get test data configuration
    TestDataConfig get_data_config() const;
    
    // Get mock configuration
    MockConfig get_mock_config() const;
    
    // Get global settings
    bool is_test_mode() const { return test_mode_; }
    std::string get_log_level() const { return log_level_; }
    bool use_mock_exchanges() const { return use_mock_exchanges_; }
    
    // Helper methods
    std::string get_test_api_key(const std::string& exchange_name) const;
    std::string get_test_api_secret(const std::string& exchange_name) const;
    std::string get_public_ws_url(const std::string& exchange_name) const;
    std::string get_private_ws_url(const std::string& exchange_name) const;
    std::string get_http_url(const std::string& exchange_name) const;
    
private:
    TestConfigManager() = default;
    ~TestConfigManager() = default;
    TestConfigManager(const TestConfigManager&) = delete;
    TestConfigManager& operator=(const TestConfigManager&) = delete;
    
    void parse_config_file(const std::string& config_file);
    void parse_section(const std::string& section_name, const std::map<std::string, std::string>& entries);
    std::string expand_variables(const std::string& value) const;
    
    bool test_mode_{true};
    std::string log_level_{"DEBUG"};
    bool use_mock_exchanges_{false};
    
    std::map<std::string, ExchangeTestConfig> exchange_configs_;
    TestScenarioConfig scenario_config_;
    TestDataConfig data_config_;
    MockConfig mock_config_;
    
    // Variable substitution
    std::map<std::string, std::string> variables_;
};

// Convenience functions for easy access
inline TestConfigManager& get_test_config() {
    return TestConfigManager::get_instance();
}

inline ExchangeTestConfig get_binance_test_config() {
    return get_test_config().get_exchange_config("BINANCE");
}

inline ExchangeTestConfig get_deribit_test_config() {
    return get_test_config().get_exchange_config("DERIBIT");
}

inline ExchangeTestConfig get_grvt_test_config() {
    return get_test_config().get_exchange_config("GRVT");
}

} // namespace test_config
