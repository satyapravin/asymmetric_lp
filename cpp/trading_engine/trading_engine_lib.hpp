#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include "../exchanges/i_exchange_oms.hpp"
#include "../exchanges/oms_factory.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include "../utils/oms/order_state.hpp"
#include "../proto/order.pb.h"

namespace trading_engine {

/**
 * Trading Engine Library
 * 
 * Core order management logic that can be used as:
 * 1. Library for testing and integration
 * 2. Standalone process for production deployment
 * 
 * Responsibilities:
 * - Connect to exchange private WebSocket streams
 * - Process order requests from ZMQ subscribers
 * - Manage order state and lifecycle
 * - Publish order events and trade executions to ZMQ
 */
class TradingEngineLib {
public:
    TradingEngineLib();
    ~TradingEngineLib();

    // Library lifecycle
    bool initialize(const std::string& config_file = "");
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void set_exchange(const std::string& exchange) { exchange_name_ = exchange; }
    void set_zmq_subscriber(std::shared_ptr<ZmqSubscriber> subscriber) { subscriber_ = subscriber; }
    void set_zmq_publisher(std::shared_ptr<ZmqPublisher> publisher) { publisher_ = publisher; }

    // Event callbacks
    using OrderEventCallback = std::function<void(const proto::OrderEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void set_order_event_callback(OrderEventCallback callback) { order_event_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }

    // Order management interface
    bool send_order(const std::string& cl_ord_id, const std::string& symbol, 
                   proto::Side side, proto::OrderType type, double qty, double price);
    bool cancel_order(const std::string& cl_ord_id);
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty);

    // Order state queries
    std::optional<OrderStateInfo> get_order_state(const std::string& cl_ord_id) const;
    std::vector<OrderStateInfo> get_active_orders() const;
    std::vector<OrderStateInfo> get_all_orders() const;

    // Statistics
    struct Statistics {
        std::atomic<uint64_t> orders_received{0};
        std::atomic<uint64_t> orders_sent_to_exchange{0};
        std::atomic<uint64_t> orders_acked{0};
        std::atomic<uint64_t> orders_filled{0};
        std::atomic<uint64_t> orders_cancelled{0};
        std::atomic<uint64_t> orders_rejected{0};
        std::atomic<uint64_t> trade_executions{0};
        std::atomic<uint64_t> zmq_messages_received{0};
        std::atomic<uint64_t> zmq_messages_sent{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<uint64_t> parse_errors{0};
        
        void reset() {
            orders_received.store(0);
            orders_sent_to_exchange.store(0);
            orders_acked.store(0);
            orders_filled.store(0);
            orders_cancelled.store(0);
            orders_rejected.store(0);
            trade_executions.store(0);
            zmq_messages_received.store(0);
            zmq_messages_sent.store(0);
            connection_errors.store(0);
            parse_errors.store(0);
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }

private:
    std::atomic<bool> running_;
    std::string exchange_name_;
    
    // Core components
    std::unique_ptr<IExchangeOMS> exchange_oms_;
    std::shared_ptr<ZmqSubscriber> subscriber_;
    std::shared_ptr<ZmqPublisher> publisher_;
    std::unique_ptr<config::ProcessConfigManager> config_manager_;
    std::unique_ptr<OrderStateMachine> order_state_machine_;
    
    // Order management
    std::map<std::string, OrderStateInfo> order_states_;
    mutable std::mutex order_states_mutex_;
    
    // Message processing
    std::thread message_processing_thread_;
    std::atomic<bool> message_processing_running_{false};
    std::queue<std::string> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_cv_;
    
    // Callbacks
    OrderEventCallback order_event_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    Statistics statistics_;
    
    // Internal methods
    void setup_exchange_oms();
    void message_processing_loop();
    void handle_order_request(const proto::OrderRequest& order_request);
    void handle_order_event(const proto::OrderEvent& order_event);
    void handle_error(const std::string& error_message);
    void publish_order_event(const proto::OrderEvent& order_event);
    void update_order_state(const std::string& cl_ord_id, proto::OrderEventType event_type);
};

} // namespace trading_engine
