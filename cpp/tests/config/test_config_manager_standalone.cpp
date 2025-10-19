#include "test_config_manager_standalone.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

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
        std::cerr << "Failed to load config: " << e.what() << std::endl;
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
                
                current_entries[key] = value;
            }
        }
    }
    
    // Process last section
    if (!current_section.empty()) {
        parse_section(current_section, current_entries);
    }
}

void TestConfigManager::parse_section(const std::string& section_name, const std::map<std::string, std::string>& entries) {
    if (section_name == "GLOBAL") {
        for (const auto& entry : entries) {
            if (entry.first == "TEST_MODE") {
                test_mode_ = (entry.second == "true");
            } else if (entry.first == "LOG_LEVEL") {
                log_level_ = expand_variables(entry.second);
            } else if (entry.first == "USE_MOCK_EXCHANGES") {
                use_mock_exchanges_ = (entry.second == "true");
            }
        }
    } else if (section_name == "BINANCE") {
        ExchangeTestConfig config;
        config.exchange_name = "BINANCE";
        config.api_key = expand_variables(entries.at("API_KEY"));
        config.api_secret = expand_variables(entries.at("API_SECRET"));
        config.public_ws_url = expand_variables(entries.at("PUBLIC_WS_URL"));
        config.private_ws_url = expand_variables(entries.at("PRIVATE_WS_URL"));
        config.http_url = expand_variables(entries.at("HTTP_URL"));
        config.testnet = (entries.at("TESTNET") == "true");
        config.asset_type = expand_variables(entries.at("ASSET_TYPE"));
        config.symbol = expand_variables(entries.at("SYMBOL"));
        config.timeout_ms = std::stoi(expand_variables(entries.at("TIMEOUT_MS")));
        config.max_retries = std::stoi(expand_variables(entries.at("MAX_RETRIES")));
        
        exchange_configs_["BINANCE"] = config;
    } else if (section_name == "DERIBIT") {
        ExchangeTestConfig config;
        config.exchange_name = "DERIBIT";
        config.api_key = expand_variables(entries.at("API_KEY"));
        config.api_secret = expand_variables(entries.at("API_SECRET"));
        config.public_ws_url = expand_variables(entries.at("PUBLIC_WS_URL"));
        config.private_ws_url = expand_variables(entries.at("PRIVATE_WS_URL"));
        config.http_url = expand_variables(entries.at("HTTP_URL"));
        config.testnet = (entries.at("TESTNET") == "true");
        config.asset_type = expand_variables(entries.at("ASSET_TYPE"));
        config.symbol = expand_variables(entries.at("SYMBOL"));
        config.timeout_ms = std::stoi(expand_variables(entries.at("TIMEOUT_MS")));
        config.max_retries = std::stoi(expand_variables(entries.at("MAX_RETRIES")));
        
        exchange_configs_["DERIBIT"] = config;
    } else if (section_name == "GRVT") {
        ExchangeTestConfig config;
        config.exchange_name = "GRVT";
        config.api_key = expand_variables(entries.at("API_KEY"));
        config.api_secret = expand_variables(entries.at("API_SECRET"));
        config.session_cookie = expand_variables(entries.at("SESSION_COOKIE"));
        config.account_id = expand_variables(entries.at("ACCOUNT_ID"));
        config.public_ws_url = expand_variables(entries.at("PUBLIC_WS_URL"));
        config.private_ws_url = expand_variables(entries.at("PRIVATE_WS_URL"));
        config.http_url = expand_variables(entries.at("HTTP_URL"));
        config.testnet = (entries.at("TESTNET") == "true");
        config.asset_type = expand_variables(entries.at("ASSET_TYPE"));
        config.symbol = expand_variables(entries.at("SYMBOL"));
        config.timeout_ms = std::stoi(expand_variables(entries.at("TIMEOUT_MS")));
        config.max_retries = std::stoi(expand_variables(entries.at("MAX_RETRIES")));
        
        exchange_configs_["GRVT"] = config;
    } else if (section_name == "TEST_SCENARIOS") {
        scenario_config_.valid_credentials_test = (entries.at("VALID_CREDENTIALS_TEST") == "true");
        scenario_config_.invalid_credentials_test = (entries.at("INVALID_CREDENTIALS_TEST") == "true");
        scenario_config_.empty_credentials_test = (entries.at("EMPTY_CREDENTIALS_TEST") == "true");
        scenario_config_.concurrent_auth_test = (entries.at("CONCURRENT_AUTH_TEST") == "true");
        scenario_config_.rate_limiting_test = (entries.at("RATE_LIMITING_TEST") == "true");
        scenario_config_.token_expiration_test = (entries.at("TOKEN_EXPIRATION_TEST") == "true");
        scenario_config_.mixed_auth_test = (entries.at("MIXED_AUTH_TEST") == "true");
    } else if (section_name == "TEST_DATA") {
        // Parse test symbols
        std::string symbols_str = expand_variables(entries.at("TEST_SYMBOLS"));
        std::istringstream symbols_stream(symbols_str);
        std::string symbol;
        while (std::getline(symbols_stream, symbol, ',')) {
            symbol.erase(0, symbol.find_first_not_of(" \t"));
            symbol.erase(symbol.find_last_not_of(" \t") + 1);
            data_config_.test_symbols.push_back(symbol);
        }
        
        // Parse test order sizes
        std::string sizes_str = expand_variables(entries.at("TEST_ORDER_SIZES"));
        std::istringstream sizes_stream(sizes_str);
        std::string size;
        while (std::getline(sizes_stream, size, ',')) {
            size.erase(0, size.find_first_not_of(" \t"));
            size.erase(size.find_last_not_of(" \t") + 1);
            data_config_.test_order_sizes.push_back(std::stod(size));
        }
        
        // Parse test prices
        std::string prices_str = expand_variables(entries.at("TEST_PRICES"));
        std::istringstream prices_stream(prices_str);
        std::string price;
        while (std::getline(prices_stream, price, ',')) {
            price.erase(0, price.find_first_not_of(" \t"));
            price.erase(price.find_last_not_of(" \t") + 1);
            data_config_.test_prices.push_back(std::stod(price));
        }
        
        // Parse test sides
        std::string sides_str = expand_variables(entries.at("TEST_SIDES"));
        std::istringstream sides_stream(sides_str);
        std::string side;
        while (std::getline(sides_stream, side, ',')) {
            side.erase(0, side.find_first_not_of(" \t"));
            side.erase(side.find_last_not_of(" \t") + 1);
            data_config_.test_sides.push_back(side);
        }
        
        // Parse test order types
        std::string types_str = expand_variables(entries.at("TEST_ORDER_TYPES"));
        std::istringstream types_stream(types_str);
        std::string type;
        while (std::getline(types_stream, type, ',')) {
            type.erase(0, type.find_first_not_of(" \t"));
            type.erase(type.find_last_not_of(" \t") + 1);
            data_config_.test_order_types.push_back(type);
        }
    } else if (section_name == "MOCK_CONFIG") {
        mock_config_.use_mock_responses = (entries.at("USE_MOCK_RESPONSES") == "true");
        mock_config_.mock_delay_ms = std::stoi(expand_variables(entries.at("MOCK_DELAY_MS")));
        mock_config_.mock_error_rate = std::stod(expand_variables(entries.at("MOCK_ERROR_RATE")));
        mock_config_.mock_fill_rate = std::stod(expand_variables(entries.at("MOCK_FILL_RATE")));
    }
}

std::string TestConfigManager::expand_variables(const std::string& value) const {
    std::string result = value;
    
    // Find ${VAR} patterns and replace with environment variables
    size_t start = 0;
    while ((start = result.find("${", start)) != std::string::npos) {
        size_t end = result.find("}", start);
        if (end != std::string::npos) {
            std::string var_name = result.substr(start + 2, end - start - 2);
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
                result.replace(start, end - start + 1, env_value);
                start += strlen(env_value);
            } else {
                start = end + 1;
            }
        } else {
            break;
        }
    }
    
    return result;
}

ExchangeTestConfig TestConfigManager::get_exchange_config(const std::string& exchange_name) const {
    auto it = exchange_configs_.find(exchange_name);
    if (it != exchange_configs_.end()) {
        return it->second;
    }
    
    // Return default config for unknown exchange
    ExchangeTestConfig default_config;
    default_config.exchange_name = exchange_name;
    default_config.api_key = "test_api_key";
    default_config.api_secret = "test_api_secret";
    default_config.session_cookie = "test_session_cookie";
    default_config.account_id = "test_account_id";
    default_config.public_ws_url = "wss://test.com/stream";
    default_config.private_ws_url = "wss://test.com";
    default_config.http_url = "https://test.com";
    default_config.testnet = true;
    default_config.asset_type = "FUTURES";
    default_config.symbol = "BTCUSDT";
    default_config.timeout_ms = 5000;
    default_config.max_retries = 3;
    
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
