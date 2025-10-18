#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <iostream>

namespace config {

class ConfigValidator {
public:
    using ValidatorFunc = std::function<bool(const std::string&)>;
    
    void add_validator(const std::string& key, ValidatorFunc validator, const std::string& error_msg) {
        validators_[key] = {validator, error_msg};
    }
    
    bool validate(const std::map<std::string, std::string>& config) const {
        for (const auto& [key, validator_info] : validators_) {
            auto it = config.find(key);
            if (it != config.end()) {
                if (!validator_info.first(it->second)) {
                    throw std::invalid_argument("Validation failed for " + key + ": " + validator_info.second);
                }
            }
        }
        return true;
    }
    
private:
    struct ValidatorInfo {
        ValidatorFunc first;
        std::string second;
    };
    std::map<std::string, ValidatorInfo> validators_;
};

class EnvironmentConfig {
public:
    static std::string get_env_var(const std::string& name, const std::string& default_value = "") {
        const char* value = std::getenv(name.c_str());
        return value ? std::string(value) : default_value;
    }
    
    static std::string expand_env_vars(const std::string& input) {
        std::string result = input;
        size_t start = 0;
        
        while ((start = result.find("${", start)) != std::string::npos) {
            size_t end = result.find("}", start);
            if (end == std::string::npos) break;
            
            std::string var_name = result.substr(start + 2, end - start - 2);
            std::string var_value = get_env_var(var_name);
            
            result.replace(start, end - start + 1, var_value);
            start += var_value.length();
        }
        
        return result;
    }
    
    static std::map<std::string, std::string> load_env_config() {
        std::map<std::string, std::string> config;
        
        // Load common environment variables
        config["BINANCE_API_KEY"] = get_env_var("BINANCE_API_KEY");
        config["BINANCE_API_SECRET"] = get_env_var("BINANCE_API_SECRET");
        config["DERIBIT_API_KEY"] = get_env_var("DERIBIT_API_KEY");
        config["DERIBIT_API_SECRET"] = get_env_var("DERIBIT_API_SECRET");
        config["GRVT_API_KEY"] = get_env_var("GRVT_API_KEY");
        config["GRVT_API_SECRET"] = get_env_var("GRVT_API_SECRET");
        
        config["LOG_LEVEL"] = get_env_var("LOG_LEVEL", "INFO");
        config["ENVIRONMENT"] = get_env_var("ENVIRONMENT", "development");
        config["DATA_DIR"] = get_env_var("DATA_DIR", "/tmp/asymmetric_lp");
        
        return config;
    }
};

class ConfigManager {
public:
    ConfigManager() {
        setup_default_validators();
    }
    
    void load_from_file(const std::string& file_path) {
        // Load INI file and expand environment variables
        // This would integrate with the existing INI loading code
        std::cout << "[CONFIG] Loading configuration from: " << file_path << std::endl;
    }
    
    void load_from_env() {
        env_config_ = EnvironmentConfig::load_env_config();
        std::cout << "[CONFIG] Loaded " << env_config_.size() << " environment variables" << std::endl;
    }
    
    std::string get(const std::string& key, const std::string& default_value = "") const {
        // Check environment first, then file config
        auto env_it = env_config_.find(key);
        if (env_it != env_config_.end()) {
            return EnvironmentConfig::expand_env_vars(env_it->second);
        }
        
        auto file_it = file_config_.find(key);
        if (file_it != file_config_.end()) {
            return EnvironmentConfig::expand_env_vars(file_it->second);
        }
        
        return default_value;
    }
    
    void set(const std::string& key, const std::string& value) {
        file_config_[key] = value;
    }
    
    bool validate() const {
        std::map<std::string, std::string> all_config;
        all_config.insert(env_config_.begin(), env_config_.end());
        all_config.insert(file_config_.begin(), file_config_.end());
        
        return validator_.validate(all_config);
    }
    
    void add_validator(const std::string& key, ConfigValidator::ValidatorFunc validator, const std::string& error_msg) {
        validator_.add_validator(key, validator, error_msg);
    }
    
    std::map<std::string, std::string> get_all() const {
        std::map<std::string, std::string> all_config;
        all_config.insert(env_config_.begin(), env_config_.end());
        all_config.insert(file_config_.begin(), file_config_.end());
        return all_config;
    }
    
private:
    void setup_default_validators() {
        // API key validation
        add_validator("BINANCE_API_KEY", 
            [](const std::string& value) { return !value.empty() && value.length() >= 10; },
            "Binance API key must be at least 10 characters");
            
        add_validator("BINANCE_API_SECRET",
            [](const std::string& value) { return !value.empty() && value.length() >= 10; },
            "Binance API secret must be at least 10 characters");
            
        add_validator("DERIBIT_API_KEY",
            [](const std::string& value) { return !value.empty() && value.length() >= 10; },
            "Deribit API key must be at least 10 characters");
            
        add_validator("DERIBIT_API_SECRET",
            [](const std::string& value) { return !value.empty() && value.length() >= 10; },
            "Deribit API secret must be at least 10 characters");
            
        // Environment validation
        add_validator("ENVIRONMENT",
            [](const std::string& value) { 
                return value == "development" || value == "staging" || value == "production"; 
            },
            "Environment must be development, staging, or production");
            
        // Log level validation
        add_validator("LOG_LEVEL",
            [](const std::string& value) {
                return value == "DEBUG" || value == "INFO" || value == "WARN" || value == "ERROR";
            },
            "Log level must be DEBUG, INFO, WARN, or ERROR");
    }
    
    std::map<std::string, std::string> env_config_;
    std::map<std::string, std::string> file_config_;
    ConfigValidator validator_;
};

// Global config manager instance
extern std::unique_ptr<ConfigManager> g_config;

// Convenience functions
inline std::string get_config(const std::string& key, const std::string& default_value = "") {
    return g_config ? g_config->get(key, default_value) : default_value;
}

inline void set_config(const std::string& key, const std::string& value) {
    if (g_config) {
        g_config->set(key, value);
    }
}

inline bool is_production() {
    return get_config("ENVIRONMENT") == "production";
}

inline bool is_development() {
    return get_config("ENVIRONMENT") == "development";
}

inline bool is_staging() {
    return get_config("ENVIRONMENT") == "staging";
}

} // namespace config
