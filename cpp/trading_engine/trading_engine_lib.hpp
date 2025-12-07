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
#include "../exchanges/i_exchange_data_fetcher.hpp"
#include "../exchanges/oms_factory.hpp"
#include "../exchanges/data_fetcher_factory.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include "../utils/oms/order_state.hpp"
#include "../exchanges/websocket/i_websocket_transport.hpp"
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
    void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport);

    // Event callbacks
    using OrderEventCallback = std::function<void(const proto::OrderEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void set_order_event_callback(OrderEventCallback callback) { order_event_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }

    // Order management interface
    
    /**
     * Send an order to the exchange
     * 
     * @param cl_ord_id Client order ID (must be unique)
     * @param symbol Trading symbol (e.g., "BTCUSDT")
     * @param side BUY or SELL
     * @param type MARKET or LIMIT
     * @param qty Order quantity
     * @param price Limit price (ignored for MARKET orders)
     * @return true if order was sent successfully, false otherwise
     * 
     * @note Order state will be tracked internally. Order events will be published via ZMQ
     *       and delivered to registered callbacks.
     * @note This method is thread-safe.
     */
    bool send_order(const std::string& cl_ord_id, const std::string& symbol, 
                   proto::Side side, proto::OrderType type, double qty, double price);
    
    /**
     * Cancel an existing order
     * 
     * @param cl_ord_id Client order ID of the order to cancel
     * @return true if cancel request was sent successfully, false otherwise
     * 
     * @note Order state will be updated to CANCELLED when exchange confirms cancellation.
     * @note This method is thread-safe.
     */
    bool cancel_order(const std::string& cl_ord_id);
    
    /**
     * Modify an existing order (replace with new price/quantity)
     * 
     * @param cl_ord_id Client order ID of the order to modify
     * @param new_price New limit price
     * @param new_qty New quantity
     * @return true if modify request was sent successfully, false otherwise
     * 
     * @note This typically cancels the old order and places a new one atomically.
     * @note Order state will be updated when exchange confirms modification.
     * @note This method is thread-safe.
     */
    bool modify_order(const std::string& cl_ord_id, double new_price, double new_qty);

    // Order state queries
    
    /**
     * Get the current state of a specific order
     * 
     * @param cl_ord_id Client order ID
     * @return OrderStateInfo if order exists, std::nullopt otherwise
     * 
     * @note This method is thread-safe and provides a snapshot of order state.
     */
    std::optional<OrderStateInfo> get_order_state(const std::string& cl_ord_id) const;
    
    /**
     * Get all currently active orders (ACKNOWLEDGED or PARTIALLY_FILLED)
     * 
     * @return Vector of active order states
     * 
     * @note This method is thread-safe and provides a snapshot of active orders.
     */
    std::vector<OrderStateInfo> get_active_orders() const;
    
    /**
     * Get all orders (active and completed)
     * 
     * @return Vector of all order states
     * 
     * @note This method is thread-safe and provides a snapshot of all orders.
     * @note Completed orders (FILLED, CANCELLED, REJECTED) are included.
     */
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
        std::atomic<uint64_t> zmq_messages_dropped{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<uint64_t> parse_errors{0};
        std::atomic<uint64_t> callback_errors{0};
        
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
            zmq_messages_dropped.store(0);
            connection_errors.store(0);
            parse_errors.store(0);
            callback_errors.store(0);
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }
    
    // Get atomic snapshot of all statistics (thread-safe)
    struct StatisticsSnapshot {
        uint64_t orders_received;
        uint64_t orders_sent_to_exchange;
        uint64_t orders_acked;
        uint64_t orders_filled;
        uint64_t orders_cancelled;
        uint64_t orders_rejected;
        uint64_t trade_executions;
        uint64_t zmq_messages_received;
        uint64_t zmq_messages_sent;
        uint64_t zmq_messages_dropped;
        uint64_t connection_errors;
        uint64_t parse_errors;
        uint64_t callback_errors;
    };
    
    StatisticsSnapshot get_statistics_snapshot() const {
        StatisticsSnapshot snap;
        snap.orders_received = statistics_.orders_received.load();
        snap.orders_sent_to_exchange = statistics_.orders_sent_to_exchange.load();
        snap.orders_acked = statistics_.orders_acked.load();
        snap.orders_filled = statistics_.orders_filled.load();
        snap.orders_cancelled = statistics_.orders_cancelled.load();
        snap.orders_rejected = statistics_.orders_rejected.load();
        snap.trade_executions = statistics_.trade_executions.load();
        snap.zmq_messages_received = statistics_.zmq_messages_received.load();
        snap.zmq_messages_sent = statistics_.zmq_messages_sent.load();
        snap.zmq_messages_dropped = statistics_.zmq_messages_dropped.load();
        snap.connection_errors = statistics_.connection_errors.load();
        snap.parse_errors = statistics_.parse_errors.load();
        snap.callback_errors = statistics_.callback_errors.load();
        return snap;
    }

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
    
    // Rate limiting
    mutable std::mutex rate_limit_mutex_;
    std::atomic<int> orders_sent_this_second_{0};
    std::chrono::steady_clock::time_point last_rate_reset_;
    int max_orders_per_second_{10}; // Configurable via config file
    
    // Rate limiting helpers
    bool check_rate_limit();
    void update_rate_limit();
    
    // Order validation
    bool validate_order(const std::string& symbol, double qty, double price, proto::OrderType type);
    
    // Reconnection handling
    void on_reconnected();
    
    // Internal methods
    void setup_exchange_oms();
    void query_open_orders_at_startup();
    void message_processing_loop();
    void handle_order_request(const proto::OrderRequest& order_request);
    void handle_order_event(const proto::OrderEvent& order_event);
    void handle_error(const std::string& error_message);
    void publish_order_event(const proto::OrderEvent& order_event);
    void update_order_state(const std::string& cl_ord_id, proto::OrderEventType event_type);
    
    // Internal helper that assumes lock is already held (prevents double locking)
    void update_order_state_internal(std::map<std::string, OrderStateInfo>::iterator it, 
                                     proto::OrderEventType event_type);
};

} // namespace trading_engine
