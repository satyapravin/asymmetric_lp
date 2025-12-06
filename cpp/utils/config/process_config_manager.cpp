#include "process_config_manager.hpp"
#include "../logging/log_helper.hpp"
#include <filesystem>
#include <stdexcept>

namespace config {

ProcessConfigManager::ProcessConfigManager() {
    // Initialize with empty configuration
}

bool ProcessConfigManager::load_config(const std::string& config_file) {
    try {
        std::string content = read_file(config_file);
        return load_config_from_string(content);
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("CONFIG", "Error loading config file " + config_file + ": " + e.what());
        return false;
    }
}

bool ProcessConfigManager::load_config_from_string(const std::string& config_content) {
    config_data_.clear();
    validation_errors_.clear();
    
    std::istringstream stream(config_content);
    std::string line;
    std::string current_section;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        if (is_section_line(line)) {
            current_section = extract_section_name(line);
            config_data_[current_section] = std::map<std::string, ConfigValue>();
        } else if (is_key_value_line(line)) {
            if (current_section.empty()) {
                validation_errors_.push_back("Key-value pair found outside of section: " + line);
                continue;
            }
            
            auto [key, value] = extract_key_value(line);
            config_data_[current_section][key] = ConfigValue(value);
        }
    }
    
    return validation_errors_.empty();
}

void ProcessConfigManager::save_config(const std::string& config_file) const {
    std::ostringstream content;
    
    for (const auto& [section, keys] : config_data_) {
        content << "[" << section << "]" << std::endl;
        for (const auto& [key, value] : keys) {
            content << key << " = " << value.as_string() << std::endl;
        }
        content << std::endl;
    }
    
    write_file(config_file, content.str());
}

ConfigValue ProcessConfigManager::get_value(const std::string& section, const std::string& key) const {
    auto section_it = config_data_.find(section);
    if (section_it == config_data_.end()) {
        return ConfigValue("");
    }
    
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return ConfigValue("");
    }
    
    return key_it->second;
}

ConfigValue ProcessConfigManager::get_value(const std::string& section, const std::string& key, const ConfigValue& default_value) const {
    auto section_it = config_data_.find(section);
    if (section_it == config_data_.end()) {
        return default_value;
    }
    
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return default_value;
    }
    
    return key_it->second;
}

std::string ProcessConfigManager::get_string(const std::string& section, const std::string& key, const std::string& default_value) const {
    return get_value(section, key, ConfigValue(default_value)).as_string();
}

int ProcessConfigManager::get_int(const std::string& section, const std::string& key, int default_value) const {
    return get_value(section, key, ConfigValue(default_value)).as_int();
}

double ProcessConfigManager::get_double(const std::string& section, const std::string& key, double default_value) const {
    return get_value(section, key, ConfigValue(default_value)).as_double();
}

bool ProcessConfigManager::get_bool(const std::string& section, const std::string& key, bool default_value) const {
    return get_value(section, key, ConfigValue(default_value)).as_bool();
}

std::vector<std::string> ProcessConfigManager::get_sections() const {
    std::vector<std::string> sections;
    for (const auto& [section, _] : config_data_) {
        sections.push_back(section);
    }
    return sections;
}

std::vector<std::string> ProcessConfigManager::get_keys(const std::string& section) const {
    std::vector<std::string> keys;
    auto section_it = config_data_.find(section);
    if (section_it != config_data_.end()) {
        for (const auto& [key, _] : section_it->second) {
            keys.push_back(key);
        }
    }
    return keys;
}

bool ProcessConfigManager::has_section(const std::string& section) const {
    return config_data_.find(section) != config_data_.end();
}

bool ProcessConfigManager::has_key(const std::string& section, const std::string& key) const {
    auto section_it = config_data_.find(section);
    if (section_it == config_data_.end()) {
        return false;
    }
    return section_it->second.find(key) != section_it->second.end();
}

void ProcessConfigManager::set_value(const std::string& section, const std::string& key, const ConfigValue& value) {
    config_data_[section][key] = value;
}

void ProcessConfigManager::set_string(const std::string& section, const std::string& key, const std::string& value) {
    set_value(section, key, ConfigValue(value));
}

void ProcessConfigManager::set_int(const std::string& section, const std::string& key, int value) {
    set_value(section, key, ConfigValue(value));
}

void ProcessConfigManager::set_double(const std::string& section, const std::string& key, double value) {
    set_value(section, key, ConfigValue(value));
}

void ProcessConfigManager::set_bool(const std::string& section, const std::string& key, bool value) {
    set_value(section, key, ConfigValue(value));
}

bool ProcessConfigManager::validate_config() const {
    // Note: validation_errors_ is mutable for const methods
    validation_errors_.clear();
    
    for (const auto& [section, keys] : config_data_) {
        if (!validate_section(section)) {
            return false;
        }
        
        for (const auto& [key, value] : keys) {
            if (!validate_key_value(section, key, value)) {
                return false;
            }
        }
    }
    
    return validation_errors_.empty();
}

std::vector<std::string> ProcessConfigManager::get_validation_errors() const {
    return validation_errors_;
}

ProcessConfigManager ProcessConfigManager::load_trader_config(const std::string& config_file) {
    ProcessConfigManager config;
    config.load_config(config_file);
    return config;
}

ProcessConfigManager ProcessConfigManager::load_quote_server_config(const std::string& exchange, const std::string& config_file) {
    ProcessConfigManager config;
    std::string file = config_file.empty() ? get_default_config_file("quote_server", exchange) : config_file;
    config.load_config(file);
    return config;
}

ProcessConfigManager ProcessConfigManager::load_trading_engine_config(const std::string& exchange, const std::string& config_file) {
    ProcessConfigManager config;
    std::string file = config_file.empty() ? get_default_config_file("trading_engine", exchange) : config_file;
    config.load_config(file);
    return config;
}

ProcessConfigManager ProcessConfigManager::load_position_server_config(const std::string& exchange, const std::string& config_file) {
    ProcessConfigManager config;
    std::string file = config_file.empty() ? get_default_config_file("position_server", exchange) : config_file;
    config.load_config(file);
    return config;
}

std::string ProcessConfigManager::get_process_name() const {
    return get_string("process", "name", "unknown");
}

std::string ProcessConfigManager::get_exchange_name() const {
    return get_string("process", "exchange", "unknown");
}

std::string ProcessConfigManager::get_log_file() const {
    return get_string("logging", "file", "logs/" + get_process_name() + ".log");
}

std::string ProcessConfigManager::get_pid_file() const {
    return get_string("process", "pid_file", "/tmp/" + get_process_name() + ".pid");
}

std::string ProcessConfigManager::get_publisher_endpoint(const std::string& endpoint_name) const {
    return get_string("zmq", endpoint_name + "_publisher", "");
}

std::string ProcessConfigManager::get_subscriber_endpoint(const std::string& endpoint_name) const {
    return get_string("zmq", endpoint_name + "_subscriber", "");
}

std::vector<std::string> ProcessConfigManager::get_all_publisher_endpoints() const {
    std::vector<std::string> endpoints;
    auto zmq_it = config_data_.find("zmq");
    if (zmq_it != config_data_.end()) {
        for (const auto& [key, value] : zmq_it->second) {
            if (key.find("_publisher") != std::string::npos) {
                endpoints.push_back(value.as_string());
            }
        }
    }
    return endpoints;
}

std::vector<std::string> ProcessConfigManager::get_all_subscriber_endpoints() const {
    std::vector<std::string> endpoints;
    auto zmq_it = config_data_.find("zmq");
    if (zmq_it != config_data_.end()) {
        for (const auto& [key, value] : zmq_it->second) {
            if (key.find("_subscriber") != std::string::npos) {
                endpoints.push_back(value.as_string());
            }
        }
    }
    return endpoints;
}

std::string ProcessConfigManager::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string ProcessConfigManager::to_lower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool ProcessConfigManager::is_section_line(const std::string& line) const {
    return line.length() >= 3 && line[0] == '[' && line[line.length() - 1] == ']';
}

bool ProcessConfigManager::is_key_value_line(const std::string& line) const {
    return line.find('=') != std::string::npos;
}

std::string ProcessConfigManager::extract_section_name(const std::string& line) const {
    return trim(line.substr(1, line.length() - 2));
}

std::pair<std::string, std::string> ProcessConfigManager::extract_key_value(const std::string& line) const {
    size_t eq_pos = line.find('=');
    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));
    return {key, value};
}

bool ProcessConfigManager::validate_section(const std::string& section) const {
    if (section.empty()) {
        validation_errors_.push_back("Empty section name");
        return false;
    }
    return true;
}

bool ProcessConfigManager::validate_key_value(const std::string& section, const std::string& key, const ConfigValue& value) const {
    if (key.empty()) {
        validation_errors_.push_back("Empty key in section [" + section + "]");
        return false;
    }
    return true;
}

std::string ProcessConfigManager::read_file(const std::string& filename) const {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void ProcessConfigManager::write_file(const std::string& filename, const std::string& content) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + filename);
    }
    
    file << content;
}

// Global configuration instances
std::unique_ptr<ProcessConfigManager> g_trader_config = nullptr;
std::unique_ptr<ProcessConfigManager> g_quote_server_config = nullptr;
std::unique_ptr<ProcessConfigManager> g_trading_engine_config = nullptr;
std::unique_ptr<ProcessConfigManager> g_position_server_config = nullptr;

void initialize_configs() {
    g_trader_config = std::make_unique<ProcessConfigManager>();
    g_quote_server_config = std::make_unique<ProcessConfigManager>();
    g_trading_engine_config = std::make_unique<ProcessConfigManager>();
    g_position_server_config = std::make_unique<ProcessConfigManager>();
}

ProcessConfigManager& get_trader_config() {
    if (!g_trader_config) {
        initialize_configs();
    }
    return *g_trader_config;
}

ProcessConfigManager& get_quote_server_config(const std::string& exchange) {
    if (!g_quote_server_config) {
        initialize_configs();
    }
    return *g_quote_server_config;
}

ProcessConfigManager& get_trading_engine_config(const std::string& exchange) {
    if (!g_trading_engine_config) {
        initialize_configs();
    }
    return *g_trading_engine_config;
}

ProcessConfigManager& get_position_server_config(const std::string& exchange) {
    if (!g_position_server_config) {
        initialize_configs();
    }
    return *g_position_server_config;
}

std::string get_config_file_path(const std::string& process_type, const std::string& exchange) {
    if (exchange.empty()) {
        return "config/" + process_type + ".ini";
    }
    return "config/" + process_type + "." + exchange + ".ini";
}

std::string get_default_config_file(const std::string& process_type, const std::string& exchange) {
    return get_config_file_path(process_type, exchange);
}

} // namespace config
