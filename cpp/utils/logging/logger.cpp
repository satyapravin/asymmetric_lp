#include "logger.hpp"
#include <iostream>

namespace logging {

std::unique_ptr<Logger> g_logger = std::make_unique<Logger>("SYSTEM");

void initialize_logging(const std::string& log_file, LogLevel min_level) {
    std::cout << "[LOGGING] Initializing logging system..." << std::endl;
    
    LogManager::get_instance().initialize(log_file, min_level);
    
    std::cout << "[LOGGING] Logging system initialized" << std::endl;
}

void cleanup_logging() {
    std::cout << "[LOGGING] Cleaning up logging system..." << std::endl;
    LogManager::get_instance().shutdown();
}

} // namespace logging
