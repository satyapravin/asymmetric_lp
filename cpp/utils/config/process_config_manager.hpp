#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace config {

// Configuration value types
enum class ConfigType {
    STRING,
    INT,
    DOUBLE,
    BOOL
};

// Configuration value wrapper
class ConfigValue {
public:
    ConfigValue() : type_(ConfigType::STRING), string_value_("") {}
    ConfigValue(const std::string& value) : type_(ConfigType::STRING), string_value_(value) {}
    ConfigValue(int value) : type_(ConfigType::INT), int_value_(value) {}
    ConfigValue(double value) : type_(ConfigType::DOUBLE), double_value_(value) {}
    ConfigValue(bool value) : type_(ConfigType::BOOL), bool_value_(value) {}
    
    // Getters
    std::string as_string() const {
        switch (type_) {
            case ConfigType::STRING: return string_value_;
            case ConfigType::INT: return std::to_string(int_value_);
            case ConfigType::DOUBLE: return std::to_string(double_value_);
            case ConfigType::BOOL: return bool_value_ ? "true" : "false";
            default: return "";
        }
    }
    
    int as_int() const {
        switch (type_) {
            case ConfigType::STRING: return std::stoi(string_value_);
            case ConfigType::INT: return int_value_;
            case ConfigType::DOUBLE: return static_cast<int>(double_value_);
            case ConfigType::BOOL: return bool_value_ ? 1 : 0;
            default: return 0;
        }
    }
    
    double as_double() const {
        switch (type_) {
            case ConfigType::STRING: return std::stod(string_value_);
            case ConfigType::INT: return static_cast<double>(int_value_);
            case ConfigType::DOUBLE: return double_value_;
            case ConfigType::BOOL: return bool_value_ ? 1.0 : 0.0;
            default: return 0.0;
        }
    }
    
    bool as_bool() const {
        switch (type_) {
            case ConfigType::STRING: 
                return string_value_ == "true" || string_value_ == "1" || string_value_ == "yes";
            case ConfigType::INT: return int_value_ != 0;
            case ConfigType::DOUBLE: return double_value_ != 0.0;
            case ConfigType::BOOL: return bool_value_;
            default: return false;
        }
    }
    
    ConfigType get_type() const { return type_; }

private:
    ConfigType type_;
    std::string string_value_;
    int int_value_;
    double double_value_;
    bool bool_value_;
};

// Process configuration manager
class ProcessConfigManager {
public:
    ProcessConfigManager();
    ~ProcessConfigManager() = default;
    
    // Configuration loading
    bool load_config(const std::string& config_file);
    bool load_config_from_string(const std::string& config_content);
    void save_config(const std::string& config_file) const;
    
    // Value access
    ConfigValue get_value(const std::string& section, const std::string& key) const;
    ConfigValue get_value(const std::string& section, const std::string& key, const ConfigValue& default_value) const;
    
    // Convenience methods
    std::string get_string(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& section, const std::string& key, int default_value = 0) const;
    double get_double(const std::string& section, const std::string& key, double default_value = 0.0) const;
    bool get_bool(const std::string& section, const std::string& key, bool default_value = false) const;
    
    // Section access
    std::vector<std::string> get_sections() const;
    std::vector<std::string> get_keys(const std::string& section) const;
    bool has_section(const std::string& section) const;
    bool has_key(const std::string& section, const std::string& key) const;
    
    // Value setting
    void set_value(const std::string& section, const std::string& key, const ConfigValue& value);
    void set_string(const std::string& section, const std::string& key, const std::string& value);
    void set_int(const std::string& section, const std::string& key, int value);
    void set_double(const std::string& section, const std::string& key, double value);
    void set_bool(const std::string& section, const std::string& key, bool value);
    
    // Configuration validation
    bool validate_config() const;
    std::vector<std::string> get_validation_errors() const;
    
    // Process-specific configuration loading
    static ProcessConfigManager load_trader_config(const std::string& config_file = "config/trader.ini");
    static ProcessConfigManager load_quote_server_config(const std::string& exchange, 
                                                       const std::string& config_file = "");
    static ProcessConfigManager load_trading_engine_config(const std::string& exchange, 
                                                          const std::string& config_file = "");
    static ProcessConfigManager load_position_server_config(const std::string& exchange, 
                                                           const std::string& config_file = "");
    
    // Utility methods
    std::string get_process_name() const;
    std::string get_exchange_name() const;
    std::string get_log_file() const;
    std::string get_pid_file() const;
    
    // ZMQ endpoint helpers
    std::string get_publisher_endpoint(const std::string& endpoint_name) const;
    std::string get_subscriber_endpoint(const std::string& endpoint_name) const;
    std::vector<std::string> get_all_publisher_endpoints() const;
    std::vector<std::string> get_all_subscriber_endpoints() const;

private:
    std::map<std::string, std::map<std::string, ConfigValue>> config_data_;
    mutable std::vector<std::string> validation_errors_;
    
    // Parsing helpers
    std::string trim(const std::string& str) const;
    std::string to_lower(const std::string& str) const;
    bool is_section_line(const std::string& line) const;
    bool is_key_value_line(const std::string& line) const;
    std::string extract_section_name(const std::string& line) const;
    std::pair<std::string, std::string> extract_key_value(const std::string& line) const;
    
    // Validation helpers
    bool validate_section(const std::string& section) const;
    bool validate_key_value(const std::string& section, const std::string& key, const ConfigValue& value) const;
    
    // File I/O helpers
    std::string read_file(const std::string& filename) const;
    void write_file(const std::string& filename, const std::string& content) const;
};

// Global configuration instances
extern std::unique_ptr<ProcessConfigManager> g_trader_config;
extern std::unique_ptr<ProcessConfigManager> g_quote_server_config;
extern std::unique_ptr<ProcessConfigManager> g_trading_engine_config;
extern std::unique_ptr<ProcessConfigManager> g_position_server_config;

// Convenience functions
void initialize_configs();
ProcessConfigManager& get_trader_config();
ProcessConfigManager& get_quote_server_config(const std::string& exchange);
ProcessConfigManager& get_trading_engine_config(const std::string& exchange);
ProcessConfigManager& get_position_server_config(const std::string& exchange);

// Configuration file paths
std::string get_config_file_path(const std::string& process_type, const std::string& exchange = "");
std::string get_default_config_file(const std::string& process_type, const std::string& exchange = "");

} // namespace config
