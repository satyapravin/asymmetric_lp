#pragma once
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace logging {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string component;
    std::string thread_id;
    uint64_t timestamp_us;
    std::map<std::string, std::string> metadata;
    
    LogEntry(LogLevel lvl, const std::string& msg, const std::string& comp = "")
        : level(lvl), message(msg), component(comp), timestamp_us(0) {
        timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Get thread ID
        std::stringstream ss;
        ss << std::this_thread::get_id();
        thread_id = ss.str();
    }
};

class LogManager {
public:
    static LogManager& get_instance() {
        static LogManager instance;
        return instance;
    }
    
    void initialize(const std::string& log_file = "", LogLevel min_level = LogLevel::INFO) {
        min_level_ = min_level;
        
        if (!log_file.empty()) {
            log_file_ = log_file;
            file_stream_.open(log_file, std::ios::app);
            if (!file_stream_.is_open()) {
                std::cerr << "[LOG_MANAGER] Failed to open log file: " << log_file << std::endl;
            }
        }
        
        running_.store(true);
        log_thread_ = std::thread(&LogManager::log_worker, this);
        
        std::cout << "[LOG_MANAGER] Initialized with level: " << level_to_string(min_level_) << std::endl;
    }
    
    void shutdown() {
        running_.store(false);
        cv_.notify_all();
        
        if (log_thread_.joinable()) {
            log_thread_.join();
        }
        
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        
        std::cout << "[LOG_MANAGER] Shutdown complete" << std::endl;
    }
    
    void log(const LogEntry& entry) {
        if (entry.level < min_level_) {
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            log_queue_.push(entry);
        }
        
        cv_.notify_one();
    }
    
    void set_level(LogLevel level) {
        min_level_ = level;
    }
    
private:
    LogManager() = default;
    ~LogManager() {
        shutdown();
    }
    
    void log_worker() {
        while (running_.load()) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !log_queue_.empty() || !running_.load(); });
            
            while (!log_queue_.empty()) {
                LogEntry entry = log_queue_.front();
                log_queue_.pop();
                lock.unlock();
                
                write_log(entry);
                
                lock.lock();
            }
        }
    }
    
    void write_log(const LogEntry& entry) {
        std::string formatted = format_log_entry(entry);
        
        // Write to console
        if (entry.level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
        
        // Write to file
        if (file_stream_.is_open()) {
            file_stream_ << formatted << std::endl;
            file_stream_.flush();
        }
    }
    
    std::string format_log_entry(const LogEntry& entry) {
        std::stringstream ss;
        
        // Timestamp
        auto time_t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(std::chrono::microseconds(entry.timestamp_us)));
        auto tm = *std::localtime(&time_t);
        
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(6) << (entry.timestamp_us % 1000000);
        
        // Level
        ss << " [" << level_to_string(entry.level) << "]";
        
        // Component
        if (!entry.component.empty()) {
            ss << " [" << entry.component << "]";
        }
        
        // Thread ID
        ss << " [T" << entry.thread_id << "]";
        
        // Message
        ss << " " << entry.message;
        
        // Metadata
        if (!entry.metadata.empty()) {
            ss << " {";
            bool first = true;
            for (const auto& [key, value] : entry.metadata) {
                if (!first) ss << ", ";
                ss << key << "=" << value;
                first = false;
            }
            ss << "}";
        }
        
        return ss.str();
    }
    
    std::string level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    LogLevel min_level_ = LogLevel::INFO;
    std::string log_file_;
    std::ofstream file_stream_;
    
    std::atomic<bool> running_{false};
    std::thread log_thread_;
    std::queue<LogEntry> log_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
};

class Logger {
public:
    Logger(const std::string& component) : component_(component) {}
    
    void debug(const std::string& message, const std::map<std::string, std::string>& metadata = {}) {
        log(LogLevel::DEBUG, message, metadata);
    }
    
    void info(const std::string& message, const std::map<std::string, std::string>& metadata = {}) {
        log(LogLevel::INFO, message, metadata);
    }
    
    void warn(const std::string& message, const std::map<std::string, std::string>& metadata = {}) {
        log(LogLevel::WARN, message, metadata);
    }
    
    void error(const std::string& message, const std::map<std::string, std::string>& metadata = {}) {
        log(LogLevel::ERROR, message, metadata);
    }
    
    void log(LogLevel level, const std::string& message, const std::map<std::string, std::string>& metadata = {}) {
        LogEntry entry(level, message, component_);
        entry.metadata = metadata;
        
        LogManager::get_instance().log(entry);
    }
    
private:
    std::string component_;
};

// Global logger instances
extern std::unique_ptr<Logger> g_logger;

// Convenience macros
#define LOG_DEBUG(msg) if (logging::g_logger) logging::g_logger->debug(msg)
#define LOG_INFO(msg) if (logging::g_logger) logging::g_logger->info(msg)
#define LOG_WARN(msg) if (logging::g_logger) logging::g_logger->warn(msg)
#define LOG_ERROR(msg) if (logging::g_logger) logging::g_logger->error(msg)

#define LOG_DEBUG_WITH_META(msg, meta) if (logging::g_logger) logging::g_logger->debug(msg, meta)
#define LOG_INFO_WITH_META(msg, meta) if (logging::g_logger) logging::g_logger->info(msg, meta)
#define LOG_WARN_WITH_META(msg, meta) if (logging::g_logger) logging::g_logger->warn(msg, meta)
#define LOG_ERROR_WITH_META(msg, meta) if (logging::g_logger) logging::g_logger->error(msg, meta)

// Initialize logging system
void initialize_logging(const std::string& log_file = "", LogLevel min_level = LogLevel::INFO);

// Cleanup logging system
void cleanup_logging();

} // namespace logging
