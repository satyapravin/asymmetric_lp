#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "../exchanges/i_exchange_pms.hpp"
#include "../exchanges/pms_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"

namespace position_server {

/**
 * Position Server Library
 * 
 * Core position and balance management logic that can be used as:
 * 1. Library for testing and integration
 * 2. Standalone process for production deployment
 * 
 * Responsibilities:
 * - Connect to exchange private WebSocket streams
 * - Process position updates and account balance changes
 * - Track position state and risk metrics
 * - Publish to ZMQ for downstream consumers
 */
class PositionServerLib {
public:
    PositionServerLib();
    ~PositionServerLib();

    // Library lifecycle
    bool initialize(const std::string& config_file = "");
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void set_exchange(const std::string& exchange) { exchange_name_ = exchange; }
    void set_zmq_publisher(std::shared_ptr<ZmqPublisher> publisher) { publisher_ = publisher; }

    // Event callbacks for testing
    using PositionUpdateCallback = std::function<void(const proto::PositionUpdate&)>;
    using BalanceUpdateCallback = std::function<void(const proto::AccountBalanceUpdate&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void set_position_update_callback(PositionUpdateCallback callback) { position_callback_ = callback; }
    void set_balance_update_callback(BalanceUpdateCallback callback) { balance_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }

    // Statistics
    struct Statistics {
        std::atomic<uint64_t> position_updates{0};
        std::atomic<uint64_t> balance_updates{0};
        std::atomic<uint64_t> zmq_messages_sent{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<uint64_t> parse_errors{0};
        
        void reset() {
            position_updates.store(0);
            balance_updates.store(0);
            zmq_messages_sent.store(0);
            connection_errors.store(0);
            parse_errors.store(0);
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }

    // Testing interface
    void simulate_position_update(const proto::PositionUpdate& position);
    void simulate_balance_update(const proto::AccountBalanceUpdate& balance);
    bool is_connected_to_exchange() const;

private:
    std::atomic<bool> running_;
    std::string exchange_name_;
    
    // Core components
    std::unique_ptr<IExchangePMS> exchange_pms_;
    std::shared_ptr<ZmqPublisher> publisher_;
    std::unique_ptr<config::ProcessConfigManager> config_manager_;
    
    // Callbacks
    PositionUpdateCallback position_callback_;
    BalanceUpdateCallback balance_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    Statistics statistics_;
    
    // Internal methods
    void setup_exchange_pms();
    void handle_position_update(const proto::PositionUpdate& position);
    void handle_balance_update(const proto::AccountBalanceUpdate& balance);
    void handle_error(const std::string& error_message);
    void publish_to_zmq(const std::string& topic, const std::string& message);
};

} // namespace position_server
