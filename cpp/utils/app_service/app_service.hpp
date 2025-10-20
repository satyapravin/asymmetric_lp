#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <chrono>
#include <signal.h>
#include <vector>
#include <map>
#include "../utils/config/process_config_manager.hpp"

namespace app_service {

/**
 * Application Service Base Class
 * 
 * Abstract base class for all server processes that provides:
 * - Common process lifecycle management
 * - Signal handling (SIGINT, SIGTERM, SIGHUP)
 * - Configuration loading
 * - Statistics reporting
 * - Graceful shutdown
 * - Daemonization support
 */
class AppService {
public:
    AppService(const std::string& service_name);
    virtual ~AppService();

    // Process lifecycle
    bool initialize(int argc, char** argv);
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void set_config_file(const std::string& config_file) { config_file_ = config_file; }
    void set_daemon_mode(bool daemon) { daemon_mode_ = daemon; }
    void set_stats_interval(int seconds) { stats_interval_seconds_ = seconds; }

    // Statistics
    struct Statistics {
        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> errors_count{0};
        std::atomic<uint64_t> connections_active{0};
        std::atomic<uint64_t> uptime_seconds{0};
        
        std::chrono::system_clock::time_point start_time;
        
        void reset() {
            messages_processed.store(0);
            errors_count.store(0);
            connections_active.store(0);
            uptime_seconds.store(0);
            start_time = std::chrono::system_clock::now();
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }

    // Event callbacks
    using ErrorCallback = std::function<void(const std::string&)>;
    using StatsCallback = std::function<void(const Statistics&)>;

    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }
    void set_stats_callback(StatsCallback callback) { stats_callback_ = callback; }

protected:
    // Pure virtual methods that must be implemented by derived classes
    virtual bool configure_service() = 0;
    virtual bool start_service() = 0;
    virtual void stop_service() = 0;
    virtual void print_service_stats() = 0;

    // Protected methods for derived classes
    void increment_message_count() { statistics_.messages_processed.fetch_add(1); }
    void increment_error_count() { statistics_.errors_count.fetch_add(1); }
    void set_connection_count(uint64_t count) { statistics_.connections_active.store(count); }
    
    config::ProcessConfigManager* get_config_manager() { return config_manager_.get(); }
    const std::string& get_service_name() const { return service_name_; }
    const std::string& get_config_file() const { return config_file_; }

private:
    std::string service_name_;
    std::string config_file_;
    bool daemon_mode_{false};
    int stats_interval_seconds_{30};
    
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    std::unique_ptr<config::ProcessConfigManager> config_manager_;
    std::thread stats_thread_;
    std::atomic<bool> stats_running_{false};
    
    Statistics statistics_;
    
    ErrorCallback error_callback_;
    StatsCallback stats_callback_;
    
    // Internal methods
    void setup_signal_handlers();
    void stats_reporting_loop();
    void handle_signal(int signal);
    void print_startup_banner();
    void print_shutdown_banner();
    
    // Static signal handler
    static std::atomic<AppService*> g_instance;
    static void signal_handler(int signal);
};

} // namespace app_service
