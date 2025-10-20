#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

// ZMQ includes
#include <zmq.hpp>
#include <zmq_addon.hpp>

// Exchange includes
#include "../exchanges/i_exchange_oms.hpp"
#include "../exchanges/i_exchange_subscriber.hpp"

// Utils includes
#include "../utils/oms/order.hpp"
#include "../utils/oms/order_state.hpp"
#include "../utils/oms/types.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/config/config_manager.hpp"
#include "../utils/logging/logger.hpp"

namespace trading_engine {

// Trading engine configuration
struct TradingEngineConfig {
    std::string exchange_name;
    std::string process_name;
    std::string pid_file;
    std::string log_file;
    exchange_config::AssetType asset_type;
    std::string api_key;
    std::string api_secret;
    bool testnet_mode;
    
    // ZMQ endpoints
    std::string order_events_pub_endpoint;
    std::string trade_events_pub_endpoint;
    std::string order_status_pub_endpoint;
    std::string trader_sub_endpoint;
    std::string position_server_sub_endpoint;
    
    // Order management
    int max_orders_per_second;
    int order_timeout_ms;
    bool retry_failed_orders;
    int max_order_retries;
    
    // Exchange-specific settings (handled by exchange interfaces)
    std::string http_base_url;
    std::string ws_private_url;
    bool enable_private_websocket;
    
    // Channel management
    bool enable_http_api;
    std::vector<std::string> private_channels;
};

// Order request from trader
struct OrderRequest {
    std::string request_id;
    std::string cl_ord_id;
    std::string symbol;
    std::string side;
    double qty;
    double price;
    std::string order_type;
    std::string time_in_force;
    uint64_t timestamp_us;
};

// Order response to trader
struct OrderResponse {
    std::string request_id;
    std::string cl_ord_id;
    std::string exchange_order_id;
    std::string status;
    std::string error_message;
    uint64_t timestamp_us;
};

// Trade execution report
struct TradeExecution {
    std::string cl_ord_id;
    std::string exchange_order_id;
    std::string trade_id;
    std::string symbol;
    std::string side;
    double qty;
    double price;
    double commission;
    uint64_t timestamp_us;
};

// Trading engine main class
class TradingEngine {
public:
    TradingEngine(const TradingEngineConfig& config);
    ~TradingEngine();
    
    // Lifecycle management
    bool initialize();
    void run();
    void shutdown();
    
    // Order management
    void process_order_request(const OrderRequest& request);
    void cancel_order(const std::string& cl_ord_id);
    void modify_order(const std::string& cl_ord_id, double new_price, double new_qty);
    
    // Event handling
    void handle_order_update(const std::string& order_id, const std::string& status);
    void handle_trade_update(const std::string& trade_id, double qty, double price);
    void handle_account_update(const std::string& account_data);
    void handle_balance_update(const std::string& balance_data);
    
    // Health monitoring
    bool is_healthy() const;
    std::map<std::string, std::string> get_health_status() const;
    std::map<std::string, double> get_performance_metrics() const;
    
    // Configuration access
    const TradingEngineConfig& get_config() const { return config_; }

private:
    // Configuration
    TradingEngineConfig config_;
    
    // Exchange interfaces (one per instance)
    std::unique_ptr<IExchangeOMS> oms_;
    std::unique_ptr<IExchangeSubscriber> subscriber_;
    
    // ZMQ communication
    std::unique_ptr<ZmqPublisher> order_events_publisher_;
    std::unique_ptr<ZmqPublisher> trade_events_publisher_;
    std::unique_ptr<ZmqPublisher> order_status_publisher_;
    std::unique_ptr<ZmqSubscriber> trader_subscriber_;
    std::unique_ptr<ZmqSubscriber> position_server_subscriber_;
    
    // Order management
    std::map<std::string, OrderRequest> pending_orders_;
    std::map<std::string, OrderResponse> order_responses_;
    std::queue<OrderRequest> order_queue_;
    std::mutex order_mutex_;
    std::condition_variable order_cv_;
    
    // Rate limiting
    std::atomic<int> orders_sent_this_second_{0};
    std::chrono::steady_clock::time_point last_rate_reset_;
    std::mutex rate_limit_mutex_;
    
    // Threading
    std::thread order_processing_thread_;
    std::thread zmq_subscriber_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Performance metrics
    std::atomic<uint64_t> total_orders_sent_{0};
    std::atomic<uint64_t> total_orders_filled_{0};
    std::atomic<uint64_t> total_orders_cancelled_{0};
    std::atomic<uint64_t> total_orders_rejected_{0};
    std::atomic<uint64_t> total_trades_executed_{0};
    
    // Internal methods
    void order_processing_loop();
    void zmq_subscriber_loop();
    void websocket_message_loop();
    void process_order_queue();
    void process_order_queue_item(const OrderRequest& request);
    bool check_rate_limit();
    void update_rate_limit();
    
    // HTTP API methods
    bool initialize_http_handler();
    
    // WebSocket methods
    bool initialize_websocket_manager();
    
    // Message handling
    void handle_trader_message(const std::string& message);
    void handle_position_server_message(const std::string& message);
    
    // Order conversion
    Order convert_to_exchange_order(const OrderRequest& request);
    OrderResponse convert_to_order_response(const std::string& request_id, 
                                          const std::string& cl_ord_id,
                                          const std::string& exchange_order_id,
                                          const std::string& status,
                                          const std::string& error_message = "");
    
    // Publishing
    void publish_order_event(const OrderResponse& response);
    void publish_trade_event(const TradeExecution& execution);
    void publish_order_status(const std::string& cl_ord_id, const std::string& status);
    void publish_account_update(const std::string& account_data);
    void publish_balance_update(const std::string& balance_data);
    
    // Error handling
    void handle_error(const std::string& error_message);
    void log_error(const std::string& error_message);
};

// OMS Factory for creating exchange-specific implementations
class OMSFactory {
public:
    static std::unique_ptr<IExchangeOMS> create_oms(const std::string& exchange_name, const TradingEngineConfig& config);
    static std::unique_ptr<IExchangeSubscriber> create_subscriber(const std::string& exchange_name, const TradingEngineConfig& config);
};

// Trading engine factory
class TradingEngineFactory {
public:
    static std::unique_ptr<TradingEngine> create_trading_engine(const std::string& exchange_name, const std::string& config_file);
    static TradingEngineConfig load_config(const std::string& exchange_name, const std::string& config_file);
    static std::vector<std::string> get_available_exchanges();
};

// Process management
class TradingEngineProcess {
public:
    TradingEngineProcess(const std::string& exchange_name, const std::string& config_file);
    ~TradingEngineProcess();
    
    bool start();
    void stop();
    bool is_running() const;
    
    // Daemon mode
    bool daemonize();
    
    // Signal handling
    static void signal_handler(int signal);
    static void setup_signal_handlers();

private:
    std::string exchange_name_;
    std::string config_file_;
    std::unique_ptr<TradingEngine> engine_;
    std::atomic<bool> running_{false};
    pid_t process_id_{0};
    
    // PID file management
    bool create_pid_file();
    void remove_pid_file();
};

} // namespace trading_engine
