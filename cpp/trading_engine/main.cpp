#include "trading_engine.hpp"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <algorithm>
#include "../utils/config/config.hpp"

namespace trading_engine {

// Global variables for signal handling
static std::atomic<bool> g_shutdown_requested{false};
static TradingEngineProcess* g_process_instance = nullptr;

TradingEngineProcess::TradingEngineProcess(const std::string& exchange_name) 
    : exchange_name_(exchange_name) {
    std::cout << "[TRADING_ENGINE_PROCESS] Creating process for exchange: " << exchange_name << std::endl;
}

TradingEngineProcess::~TradingEngineProcess() {
    stop();
}

bool TradingEngineProcess::start() {
    std::cout << "[TRADING_ENGINE_PROCESS] Starting trading engine process..." << std::endl;
    
    try {
        // Set up signal handlers
        setup_signal_handlers();
        g_process_instance = this;
        
        // Create PID file
        if (!create_pid_file()) {
            std::cerr << "[TRADING_ENGINE_PROCESS] Failed to create PID file" << std::endl;
            return false;
        }
        
        // Create trading engine
        engine_ = TradingEngineFactory::create_trading_engine(exchange_name_);
        if (!engine_) {
            std::cerr << "[TRADING_ENGINE_PROCESS] Failed to create trading engine" << std::endl;
            return false;
        }
        
        // Initialize trading engine
        if (!engine_->initialize()) {
            std::cerr << "[TRADING_ENGINE_PROCESS] Failed to initialize trading engine" << std::endl;
            return false;
        }
        
        // Start trading engine
        running_ = true;
        process_id_ = getpid();
        
        std::cout << "[TRADING_ENGINE_PROCESS] Trading engine process started successfully" << std::endl;
        std::cout << "[TRADING_ENGINE_PROCESS] Process ID: " << process_id_ << std::endl;
        
        // Run trading engine
        engine_->run();
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE_PROCESS] Exception during startup: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngineProcess::stop() {
    std::cout << "[TRADING_ENGINE_PROCESS] Stopping trading engine process..." << std::endl;
    
    running_ = false;
    g_shutdown_requested = true;
    
    if (engine_) {
        engine_->shutdown();
        engine_.reset();
    }
    
    remove_pid_file();
    
    std::cout << "[TRADING_ENGINE_PROCESS] Trading engine process stopped" << std::endl;
}

bool TradingEngineProcess::is_running() const {
    return running_;
}

void TradingEngineProcess::signal_handler(int signal) {
    std::cout << "[TRADING_ENGINE_PROCESS] Received signal: " << signal << std::endl;
    
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            std::cout << "[TRADING_ENGINE_PROCESS] Shutdown signal received" << std::endl;
            g_shutdown_requested = true;
            if (g_process_instance) {
                g_process_instance->stop();
            }
            break;
            
        case SIGHUP:
            std::cout << "[TRADING_ENGINE_PROCESS] Reload signal received" << std::endl;
            // TODO: Implement configuration reload
            break;
            
        case SIGUSR1:
            std::cout << "[TRADING_ENGINE_PROCESS] Status signal received" << std::endl;
            if (g_process_instance && g_process_instance->engine_) {
                auto health = g_process_instance->engine_->get_health_status();
                auto metrics = g_process_instance->engine_->get_performance_metrics();
                
                std::cout << "[TRADING_ENGINE_PROCESS] Health Status:" << std::endl;
                for (const auto& [key, value] : health) {
                    std::cout << "  " << key << ": " << value << std::endl;
                }
                
                std::cout << "[TRADING_ENGINE_PROCESS] Performance Metrics:" << std::endl;
                for (const auto& [key, value] : metrics) {
                    std::cout << "  " << key << ": " << value << std::endl;
                }
            }
            break;
            
        default:
            std::cout << "[TRADING_ENGINE_PROCESS] Unknown signal: " << signal << std::endl;
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
            std::cerr << "[TRADING_ENGINE_PROCESS] Failed to open PID file: " 
                      << engine_->get_config().pid_file << std::endl;
            return false;
        }
        
        pid_file << getpid() << std::endl;
        pid_file.close();
        
        std::cout << "[TRADING_ENGINE_PROCESS] PID file created: " 
                  << engine_->get_config().pid_file << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE_PROCESS] Exception creating PID file: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngineProcess::remove_pid_file() {
    try {
        if (unlink(engine_->get_config().pid_file.c_str()) == 0) {
            std::cout << "[TRADING_ENGINE_PROCESS] PID file removed: " 
                      << engine_->get_config().pid_file << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE_PROCESS] Exception removing PID file: " << e.what() << std::endl;
    }
}

bool TradingEngineProcess::daemonize() {
    std::cout << "[TRADING_ENGINE_PROCESS] Daemonizing process..." << std::endl;
    
    // Fork first time
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[TRADING_ENGINE_PROCESS] First fork failed" << std::endl;
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Child process continues
    if (setsid() < 0) {
        std::cerr << "[TRADING_ENGINE_PROCESS] setsid failed" << std::endl;
        return false;
    }
    
    // Fork second time
    pid = fork();
    if (pid < 0) {
        std::cerr << "[TRADING_ENGINE_PROCESS] Second fork failed" << std::endl;
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Change working directory
    if (chdir("/") < 0) {
        std::cerr << "[TRADING_ENGINE_PROCESS] chdir failed" << std::endl;
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
    
    std::cout << "[TRADING_ENGINE_PROCESS] Process daemonized successfully" << std::endl;
    return true;
}

} // namespace trading_engine

// Main function
int main(int argc, char* argv[]) {
    std::cout << "=== Trading Engine Process ===" << std::endl;
    
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
        std::cerr << "Usage: " << argv[0] << " -c <path/to/config.ini> [--daemon]" << std::endl;
        std::cerr << "Available exchanges: BINANCE, DERIBIT, GRVT" << std::endl;
        return 1;
    }

    // Read configuration
    load_from_ini(config_file, cfg);
    
    // Validate required configuration
    if (cfg.exchanges_csv.empty()) {
        std::cerr << "Config missing required key: EXCHANGES" << std::endl;
        return 1;
    }
    
    std::string exchange_name = cfg.exchanges_csv;
    std::transform(exchange_name.begin(), exchange_name.end(), exchange_name.begin(), ::toupper);
    
    std::cout << "Starting trading engine for exchange: " << exchange_name << std::endl;
    if (daemon_mode) {
        std::cout << "Running in daemon mode" << std::endl;
    }
    
    try {
        // Create trading engine process
        trading_engine::TradingEngineProcess process(exchange_name);
        
        // Daemonize if requested
        if (daemon_mode) {
            if (!process.daemonize()) {
                std::cerr << "Failed to daemonize process" << std::endl;
                return 1;
            }
        }
        
        // Start the process
        if (!process.start()) {
            std::cerr << "Failed to start trading engine process" << std::endl;
            return 1;
        }
        
        // Wait for shutdown signal
        while (!trading_engine::g_shutdown_requested && process.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Trading engine process completed" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
}
