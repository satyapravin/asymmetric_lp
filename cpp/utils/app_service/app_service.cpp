#include "app_service.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace app_service {

// Static member initialization
std::atomic<AppService*> AppService::g_instance{nullptr};

AppService::AppService(const std::string& service_name) 
    : service_name_(service_name), running_(false), initialized_(false) {
    statistics_.reset();
}

AppService::~AppService() {
    stop();
}

bool AppService::initialize(int argc, char** argv) {
    if (initialized_.load()) {
        return true;
    }

    print_startup_banner();

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--config" && i + 1 < argc) {
            config_file_ = argv[++i];
        } else if (arg == "--daemon") {
            daemon_mode_ = true;
        } else if (arg == "--stats-interval" && i + 1 < argc) {
            stats_interval_seconds_ = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << service_name_ << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --config <file>     Configuration file path" << std::endl;
            std::cout << "  --daemon           Run as daemon" << std::endl;
            std::cout << "  --stats-interval <seconds>  Statistics reporting interval" << std::endl;
            std::cout << "  --help             Show this help message" << std::endl;
            return false;
        }
    }

    // Set default config file if not provided
    if (config_file_.empty()) {
        config_file_ = service_name_ + ".ini";
    }

    std::cout << "[APP_SERVICE] Service: " << service_name_ << std::endl;
    std::cout << "[APP_SERVICE] Config file: " << config_file_ << std::endl;
    std::cout << "[APP_SERVICE] Daemon mode: " << (daemon_mode_ ? "enabled" : "disabled") << std::endl;

    // Initialize configuration manager
    config_manager_ = std::make_unique<config::ProcessConfigManager>();
    if (!config_manager_->load_config(config_file_)) {
        std::cerr << "[APP_SERVICE] Failed to load configuration from " << config_file_ << std::endl;
        return false;
    }

    // Setup signal handlers
    setup_signal_handlers();

    // Configure the specific service
    if (!configure_service()) {
        std::cerr << "[APP_SERVICE] Service configuration failed" << std::endl;
        return false;
    }

    initialized_.store(true);
    std::cout << "[APP_SERVICE] Service initialized successfully" << std::endl;
    return true;
}

void AppService::start() {
    if (!initialized_.load()) {
        std::cerr << "[APP_SERVICE] Service not initialized" << std::endl;
        return;
    }

    if (running_.load()) {
        std::cout << "[APP_SERVICE] Service already running" << std::endl;
        return;
    }

    // Daemonize if requested
    if (daemon_mode_) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "[APP_SERVICE] Failed to fork for daemonization" << std::endl;
            return;
        }
        
        if (pid > 0) {
            // Parent process - exit
            std::cout << "[APP_SERVICE] Daemon started with PID: " << pid << std::endl;
            exit(0);
        }
        
        // Child process continues
        setsid();
        chdir("/");
        
        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // Redirect to /dev/null
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
    }

    // Start statistics reporting thread
    stats_running_.store(true);
    stats_thread_ = std::thread(&AppService::stats_reporting_loop, this);

    // Start the specific service
    if (!start_service()) {
        std::cerr << "[APP_SERVICE] Failed to start service" << std::endl;
        stop();
        return;
    }

    running_.store(true);
    statistics_.start_time = std::chrono::system_clock::now();
    
    std::cout << "[APP_SERVICE] Service started successfully" << std::endl;

    // Main processing loop
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Update uptime
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - statistics_.start_time);
        statistics_.uptime_seconds.store(uptime.count());
    }
}

void AppService::stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "[APP_SERVICE] Stopping service..." << std::endl;
    
    running_.store(false);
    
    // Stop statistics thread
    stats_running_.store(false);
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    
    // Stop the specific service
    stop_service();
    
    print_shutdown_banner();
}

void AppService::setup_signal_handlers() {
    // Set global instance for signal handler
    g_instance.store(this);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGUSR1, signal_handler);  // For statistics dump
}

void AppService::stats_reporting_loop() {
    while (stats_running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(stats_interval_seconds_));
        
        if (running_.load()) {
            print_service_stats();
            
            if (stats_callback_) {
                stats_callback_(statistics_);
            }
        }
    }
}

void AppService::signal_handler(int signal) {
    AppService* instance = g_instance.load();
    if (instance) {
        instance->handle_signal(signal);
    }
}

void AppService::handle_signal(int signal) {
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            std::cout << "[APP_SERVICE] Received signal " << signal << ", shutting down..." << std::endl;
            stop();
            break;
            
        case SIGHUP:
            std::cout << "[APP_SERVICE] Received SIGHUP, reloading configuration..." << std::endl;
            // TODO: Implement configuration reload
            break;
            
        case SIGUSR1:
            std::cout << "[APP_SERVICE] Received SIGUSR1, dumping statistics..." << std::endl;
            print_service_stats();
            break;
            
        default:
            std::cout << "[APP_SERVICE] Received unknown signal " << signal << std::endl;
            break;
    }
}

void AppService::print_startup_banner() {
    std::cout << "=========================================" << std::endl;
    std::cout << "  " << service_name_ << " Service Starting" << std::endl;
    std::cout << "=========================================" << std::endl;
}

void AppService::print_shutdown_banner() {
    std::cout << "=========================================" << std::endl;
    std::cout << "  " << service_name_ << " Service Stopped" << std::endl;
    std::cout << "=========================================" << std::endl;
}

} // namespace app_service
