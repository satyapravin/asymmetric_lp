#include "test_config_manager.hpp"
#include <algorithm>
#include <cctype>

namespace test_config {

TestConfigManager& TestConfigManager::get_instance() {
    static TestConfigManager instance;
    return instance;
}

bool TestConfigManager::load_config(const std::string& config_file) {
    try {
        parse_config_file(config_file);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[TEST_CONFIG] Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

void TestConfigManager::parse_config_file(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }
    
    std::string line;
    std::string current_section;
    std::map<std::string, std::string> current_entries;
    
    while (std::getline(file, line)) {
        // Remove comments and whitespace
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            // Process previous section
            if (!current_section.empty()) {
                parse_section(current_section, current_entries);
            }
            
            // Start new section
            current_section = line.substr(1, line.length() - 2);
            std::transform(current_section.begin(), current_section.end(), 
                          current_section.begin(), ::toupper);
            current_entries.clear();
        } else {
            // Parse key=value pair
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Expand variables
                value = expand_variables(value);
                
                current_entries[key] = value;
            }
        }
    }
    
    // Process last section
    if (!current_section.empty()) {
        parse_section(current_section, current_entries);
    }
}

void TestConfigManager::parse_section(const std::string& section_name, 
                                     const std::map<std::string, std::string>& entries) {
    if (section_name == "GLOBAL") {
        for (const auto& [key, value] : entries) {
            if (key == "TEST_MODE") {
                test_mode_ = (value == "true" || value == "1");
            } else if (key == "LOG_LEVEL") {
                log_level_ = value;
            } else if (key == "USE_MOCK_EXCHANGES") {
                use_mock_exchanges_ = (value == "true" || value == "1");
            } else {
                // Store as variable for substitution
                variables_[key] = value;
            }
        }
    } else if (section_name == "BINANCE" || section_name == "DERIBIT" || section_name == "GRVT") {
        ExchangeTestConfig config;
        config.exchange_name = section_name;
        
        for (const auto& [key, value] : entries) {
            if (key == "API_KEY") config.api_key = value;
            else if (key == "API_SECRET") config.api_secret = value;
            else if (key == "PUBLIC_WS_URL") config.public_ws_url = value;
            else if (key == "PRIVATE_WS_URL") config.private_ws_url = value;
            else if (key == "HTTP_URL") config.http_url = value;
            else if (key == "TESTNET") config.testnet = (value == "true" || value == "1");
            else if (key == "ASSET_TYPE") config.asset_type = value;
            else if (key == "SYMBOL") config.symbol = value;
            else if (key == "TEST_TIMEOUT_MS") config.timeout_ms = std::stoi(value);
            else if (key == "MAX_RETRIES") config.max_retries = std::stoi(value);
        }
        
        exchange_configs_[section_name] = config;
    } else if (section_name == "TEST_SCENARIOS") {
        for (const auto& [key, value] : entries) {
            bool enabled = (value == "true" || value == "1");
            
            if (key == "VALID_CREDENTIALS_TEST") scenario_config_.valid_credentials_test = enabled;
            else if (key == "INVALID_CREDENTIALS_TEST") scenario_config_.invalid_credentials_test = enabled;
            else if (key == "EMPTY_CREDENTIALS_TEST") scenario_config_.empty_credentials_test = enabled;
            else if (key == "CONCURRENT_AUTH_TEST") scenario_config_.concurrent_auth_test = enabled;
            else if (key == "RATE_LIMITING_TEST") scenario_config_.rate_limiting_test = enabled;
            else if (key == "TOKEN_EXPIRATION_TEST") scenario_config_.token_expiration_test = enabled;
            else if (key == "MIXED_AUTH_TEST") scenario_config_.mixed_auth_test = enabled;
        }
    } else if (section_name == "TEST_DATA") {
        for (const auto& [key, value] : entries) {
            if (key == "TEST_SYMBOLS") {
                std::stringstream ss(value);
                std::string symbol;
                while (std::getline(ss, symbol, ',')) {
                    symbol.erase(0, symbol.find_first_not_of(" \t"));
                    symbol.erase(symbol.find_last_not_of(" \t") + 1);
                    data_config_.test_symbols.push_back(symbol);
                }
            } else if (key == "TEST_ORDER_SIZES") {
                std::stringstream ss(value);
                std::string size_str;
                while (std::getline(ss, size_str, ',')) {
                    size_str.erase(0, size_str.find_first_not_of(" \t"));
                    size_str.erase(size_str.find_last_not_of(" \t") + 1);
                    data_config_.test_order_sizes.push_back(std::stod(size_str));
                }
            } else if (key == "TEST_PRICES") {
                std::stringstream ss(value);
                std::string price_str;
                while (std::getline(ss, price_str, ',')) {
                    price_str.erase(0, price_str.find_first_not_of(" \t"));
                    price_str.erase(price_str.find_last_not_of(" \t") + 1);
                    data_config_.test_prices.push_back(std::stod(price_str));
                }
            } else if (key == "TEST_SIDES") {
                std::stringstream ss(value);
                std::string side;
                while (std::getline(ss, side, ',')) {
                    side.erase(0, side.find_first_not_of(" \t"));
                    side.erase(side.find_last_not_of(" \t") + 1);
                    data_config_.test_sides.push_back(side);
                }
            } else if (key == "TEST_ORDER_TYPES") {
                std::stringstream ss(value);
                std::string type;
                while (std::getline(ss, type, ',')) {
                    type.erase(0, type.find_first_not_of(" \t"));
                    type.erase(type.find_last_not_of(" \t") + 1);
                    data_config_.test_order_types.push_back(type);
                }
            }
        }
    } else if (section_name == "MOCK_CONFIG") {
        for (const auto& [key, value] : entries) {
            if (key == "USE_MOCK_RESPONSES") {
                mock_config_.use_mock_responses = (value == "true" || value == "1");
            } else if (key == "MOCK_DELAY_MS") {
                mock_config_.mock_delay_ms = std::stoi(value);
            } else if (key == "MOCK_ERROR_RATE") {
                mock_config_.mock_error_rate = std::stod(value);
            } else if (key == "MOCK_FILL_RATE") {
                mock_config_.mock_fill_rate = std::stod(value);
            }
        }
    }
}

std::string TestConfigManager::expand_variables(const std::string& value) const {
    std::string result = value;
    
    for (const auto& [var_name, var_value] : variables_) {
        std::string placeholder = "${" + var_name + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), var_value);
            pos += var_value.length();
        }
    }
    
    return result;
}

ExchangeTestConfig TestConfigManager::get_exchange_config(const std::string& exchange_name) const {
    auto it = exchange_configs_.find(exchange_name);
    if (it != exchange_configs_.end()) {
        return it->second;
    }
    
    // Return default config if not found
    ExchangeTestConfig default_config;
    default_config.exchange_name = exchange_name;
    return default_config;
}

TestScenarioConfig TestConfigManager::get_scenario_config() const {
    return scenario_config_;
}

TestDataConfig TestConfigManager::get_data_config() const {
    return data_config_;
}

MockConfig TestConfigManager::get_mock_config() const {
    return mock_config_;
}

std::string TestConfigManager::get_test_api_key(const std::string& exchange_name) const {
    return get_exchange_config(exchange_name).api_key;
}

std::string TestConfigManager::get_test_api_secret(const std::string& exchange_name) const {
    return get_exchange_config(exchange_name).api_secret;
}

std::string TestConfigManager::get_public_ws_url(const std::string& exchange_name) const {
    return get_exchange_config(exchange_name).public_ws_url;
}

std::string TestConfigManager::get_private_ws_url(const std::string& exchange_name) const {
    return get_exchange_config(exchange_name).private_ws_url;
}

std::string TestConfigManager::get_http_url(const std::string& exchange_name) const {
    return get_exchange_config(exchange_name).http_url;
}

} // namespace test_config
