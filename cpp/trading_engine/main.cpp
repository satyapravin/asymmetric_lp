#include "trading_engine.hpp"
#include "../utils/logging/log_helper.hpp"
#include <fstream>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <algorithm>
#include "../utils/config/config.hpp"
#include <thread>
#include <chrono>

namespace trading_engine {

// Global variables for signal handling
static std::atomic<bool> g_shutdown_requested{false};
static TradingEngineProcess* g_process_instance = nullptr;

TradingEngineProcess::TradingEngineProcess(const std::string& exchange_name, const std::string& config_file) 
    : exchange_name_(exchange_name), config_file_(config_file) {
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Creating process for exchange: " + exchange_name);
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Using config file: " + config_file);
}

TradingEngineProcess::~TradingEngineProcess() {
    stop();
}

bool TradingEngineProcess::start() {
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Starting trading engine process...");
    
    try {
        // Set up signal handlers
        setup_signal_handlers();
        g_process_instance = this;
        
        // Create PID file
        if (!create_pid_file()) {
            LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Failed to create PID file");
            return false;
        }
        
        // Create trading engine
        engine_ = TradingEngineFactory::create_trading_engine(exchange_name_, config_file_);
        if (!engine_) {
            LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Failed to create trading engine");
            return false;
        }
        
        // Initialize trading engine
        if (!engine_->initialize()) {
            LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Failed to initialize trading engine");
            return false;
        }
        
        // Start trading engine
        running_ = true;
        process_id_ = getpid();
        
        LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Trading engine process started successfully");
        LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Process ID: " + std::to_string(process_id_));
        
        // Run trading engine
        engine_->run();
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Exception during startup: " + std::string(e.what()));
        return false;
    }
}

void TradingEngineProcess::stop() {
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Stopping trading engine process...");
    
    running_ = false;
    g_shutdown_requested = true;
    
    if (engine_) {
        engine_->shutdown();
        engine_.reset();
    }
    
    remove_pid_file();
    
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Trading engine process stopped");
}

bool TradingEngineProcess::is_running() const {
    return running_;
}

void TradingEngineProcess::signal_handler(int signal) {
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Received signal: " + std::to_string(signal));
    
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Shutdown signal received");
            g_shutdown_requested = true;
            if (g_process_instance) {
                g_process_instance->stop();
            }
            break;
            
        case SIGHUP:
            LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Reload signal received");
            // Configuration reload functionality will be implemented when needed
            break;
            
        case SIGUSR1:
            LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Status signal received");
            if (g_process_instance && g_process_instance->engine_) {
                auto health = g_process_instance->engine_->get_health_status();
                auto metrics = g_process_instance->engine_->get_performance_metrics();
                
                LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Health Status:");
                for (const auto& [key, value] : health) {
                    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "  " + key + ": " + value);
                }
                
                LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Performance Metrics:");
                for (const auto& [key, value] : metrics) {
                    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "  " + key + ": " + value);
                }
            }
            break;
            
        default:
            LOG_WARN_COMP("TRADING_ENGINE_PROCESS", "Unknown signal: " + std::to_string(signal));
            break;
    }
}

void TradingEngineProcess::setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE
}

bool TradingEngineProcess::create_pid_file() {
    try {
        std::ofstream pid_file(engine_->get_config().pid_file);
        if (!pid_file.is_open()) {
            LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Failed to open PID file: " + engine_->get_config().pid_file);
            return false;
        }
        
        pid_file << getpid() << std::endl;
        pid_file.close();
        
        LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "PID file created: " + engine_->get_config().pid_file);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Exception creating PID file: " + std::string(e.what()));
        return false;
    }
}

void TradingEngineProcess::remove_pid_file() {
    try {
        if (unlink(engine_->get_config().pid_file.c_str()) == 0) {
            LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "PID file removed: " + engine_->get_config().pid_file);
        }
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Exception removing PID file: " + std::string(e.what()));
    }
}

bool TradingEngineProcess::daemonize() {
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Daemonizing process...");
    
    // Fork first time
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "First fork failed");
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Child process continues
    if (setsid() < 0) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "setsid failed");
        return false;
    }
    
    // Fork second time
    pid = fork();
    if (pid < 0) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "Second fork failed");
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Change working directory
    if (chdir("/") < 0) {
        LOG_ERROR_COMP("TRADING_ENGINE_PROCESS", "chdir failed");
        return false;
    }
    
    // Close file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect to /dev/null
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
    
    LOG_INFO_COMP("TRADING_ENGINE_PROCESS", "Process daemonized successfully");
    return true;
}

} // namespace trading_engine

// Main function
int main(int argc, char* argv[]) {
    LOG_INFO_COMP("TRADING_ENGINE", "=== Trading Engine Process ===");
    
    // Load configuration from command line
    AppConfig cfg;
    std::string config_file;
    bool daemon_mode = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg.rfind("--config=", 0) == 0) {
            config_file = arg.substr(std::string("--config=").size());
        } else if (arg == "--daemon") {
            daemon_mode = true;
        }
    }
    
    if (config_file.empty()) {
        LOG_ERROR_COMP("TRADING_ENGINE", "Usage: " + std::string(argv[0]) + " -c <path/to/config.ini> [--daemon]");
        LOG_ERROR_COMP("TRADING_ENGINE", "Example: " + std::string(argv[0]) + " -c /etc/trading_engine/trading_engine_binance.ini");
        return 1;
    }

    // Read configuration
    load_from_ini(config_file, cfg);
    
    // Validate required configuration
    if (cfg.exchanges_csv.empty()) {
        LOG_ERROR_COMP("TRADING_ENGINE", "Config missing required key: EXCHANGES");
        return 1;
    }
    
    std::string exchange_name = cfg.exchanges_csv;
    std::transform(exchange_name.begin(), exchange_name.end(), exchange_name.begin(), ::toupper);
    
    LOG_INFO_COMP("TRADING_ENGINE", "Starting trading engine for exchange: " + exchange_name);
    if (daemon_mode) {
        LOG_INFO_COMP("TRADING_ENGINE", "Running in daemon mode");
    }
    
    try {
        // Create trading engine process
        trading_engine::TradingEngineProcess process(exchange_name, config_file);
        
        // Daemonize if requested
        if (daemon_mode) {
            if (!process.daemonize()) {
                LOG_ERROR_COMP("TRADING_ENGINE", "Failed to daemonize process");
                return 1;
            }
        }
        
        // Start the process
        if (!process.start()) {
            LOG_ERROR_COMP("TRADING_ENGINE", "Failed to start trading engine process");
            return 1;
        }
        
        // Wait for shutdown signal
        while (!trading_engine::g_shutdown_requested && process.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        LOG_INFO_COMP("TRADING_ENGINE", "Trading engine process completed");
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("TRADING_ENGINE", "Exception in main: " + std::string(e.what()));
        return 1;
    }
}
