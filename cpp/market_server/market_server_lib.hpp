#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "../exchanges/i_exchange_subscriber.hpp"
#include "../exchanges/subscriber_factory.hpp"
#include "../exchanges/websocket/i_websocket_transport.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"

namespace market_server {

/**
 * Market Server Library
 * 
 * Core market data processing logic that can be used as:
 * 1. Library for testing and integration
 * 2. Standalone process for production deployment
 * 
 * Responsibilities:
 * - Connect to exchange WebSocket streams
 * - Process market data (orderbook, trades)
 * - Normalize data across exchanges
 * - Publish to ZMQ for downstream consumers
 */
class MarketServerLib {
public:
    MarketServerLib();
    ~MarketServerLib();

    // Library lifecycle
    bool initialize(const std::string& config_file = "");
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void set_exchange(const std::string& exchange) { exchange_name_ = exchange; }
    void set_symbol(const std::string& symbol) { symbol_ = symbol; }
    void set_zmq_publisher(std::shared_ptr<ZmqPublisher> publisher) { publisher_ = publisher; }

    // Event callbacks for testing
    using MarketDataCallback = std::function<void(const proto::OrderBookSnapshot&)>;
    using TradeCallback = std::function<void(const proto::Trade&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void set_market_data_callback(MarketDataCallback callback) { market_data_callback_ = callback; }
    void set_trade_callback(TradeCallback callback) { trade_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }

    // Statistics
    struct Statistics {
        std::atomic<uint64_t> orderbook_updates{0};
        std::atomic<uint64_t> trade_updates{0};
        std::atomic<uint64_t> zmq_messages_sent{0};
        std::atomic<uint64_t> zmq_messages_dropped{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<uint64_t> parse_errors{0};
        
        void reset() {
            orderbook_updates.store(0);
            trade_updates.store(0);
            zmq_messages_sent.store(0);
            zmq_messages_dropped.store(0);
            connection_errors.store(0);
            parse_errors.store(0);
        }
    };

    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }

    // Testing interface
    bool is_connected_to_exchange() const;
    void set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport);

private:
    std::atomic<bool> running_;
    std::string exchange_name_;
    std::string symbol_;
    
    // Core components
    std::unique_ptr<IExchangeSubscriber> exchange_subscriber_;
    std::shared_ptr<ZmqPublisher> publisher_;
    std::unique_ptr<config::ProcessConfigManager> config_manager_;
    std::unique_ptr<websocket_transport::IWebSocketTransport> custom_transport_;
    
    // Callbacks
    MarketDataCallback market_data_callback_;
    TradeCallback trade_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    Statistics statistics_;
    
    // Internal methods
    void setup_exchange_subscriber();
    void handle_orderbook_update(const proto::OrderBookSnapshot& orderbook);
    void handle_trade_update(const proto::Trade& trade);
    void handle_error(const std::string& error_message);
    void publish_to_zmq(const std::string& topic, const std::string& message);
};

} // namespace market_server
