#include "config_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace config {

std::unique_ptr<ConfigManager> g_config = std::make_unique<ConfigManager>();

void initialize_config(const std::string& config_file = "") {
    std::cout << "[CONFIG] Initializing configuration manager..." << std::endl;
    
    // Load environment variables first
    g_config->load_from_env();
    
    // Load from file if provided
    if (!config_file.empty()) {
        g_config->load_from_file(config_file);
    }
    
    // Validate configuration
    try {
        if (g_config->validate()) {
            std::cout << "[CONFIG] Configuration validation passed" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[CONFIG] Configuration validation failed: " << e.what() << std::endl;
        throw;
    }
    
    // Print configuration summary (without sensitive data)
    std::cout << "[CONFIG] Environment: " << get_config("ENVIRONMENT", "development") << std::endl;
    std::cout << "[CONFIG] Log Level: " << get_config("LOG_LEVEL", "INFO") << std::endl;
    std::cout << "[CONFIG] Data Directory: " << get_config("DATA_DIR", "/tmp/asymmetric_lp") << std::endl;
    
    // Check API keys (without printing them)
    bool has_binance = !get_config("BINANCE_API_KEY").empty();
    bool has_deribit = !get_config("DERIBIT_API_KEY").empty();
    bool has_grvt = !get_config("GRVT_API_KEY").empty();
    
    std::cout << "[CONFIG] API Keys - Binance: " << (has_binance ? "✓" : "✗") 
              << " Deribit: " << (has_deribit ? "✓" : "✗")
              << " GRVT: " << (has_grvt ? "✓" : "✗") << std::endl;
}

void cleanup_config() {
    g_config.reset();
}

} // namespace config
