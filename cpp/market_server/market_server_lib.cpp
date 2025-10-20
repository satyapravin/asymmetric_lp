#include "market_server_lib.hpp"
#include "../exchanges/subscriber_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include <iostream>
#include <thread>

namespace market_server {

MarketServerLib::MarketServerLib() 
    : running_(false), exchange_name_("binance"), symbol_("BTCUSDT") {
}

MarketServerLib::~MarketServerLib() {
    stop();
}

bool MarketServerLib::initialize(const std::string& config_file) {
    if (config_file.empty()) {
        std::cout << "[MARKET_SERVER_LIB] Using default configuration" << std::endl;
    } else {
        std::cout << "[MARKET_SERVER_LIB] Loading configuration from: " << config_file << std::endl;
        config_manager_ = std::make_unique<config::ProcessConfigManager>();
        if (!config_manager_->load_config(config_file)) {
            std::cerr << "[MARKET_SERVER_LIB] Failed to load configuration from: " << config_file << std::endl;
            return false;
        }
        
        // Load configuration values
        exchange_name_ = config_manager_->get_string("market_server.exchange", "binance");
        symbol_ = config_manager_->get_string("market_server.symbol", "BTCUSDT");
    }
    
    // Initialize ZMQ publisher
    publisher_ = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5555");
    
    // Setup exchange subscriber
    setup_exchange_subscriber();
    
    std::cout << "[MARKET_SERVER_LIB] Initialized with exchange: " << exchange_name_ 
              << ", symbol: " << symbol_ << std::endl;
    return true;
}

void MarketServerLib::start() {
    if (running_.load()) {
        std::cout << "[MARKET_SERVER_LIB] Already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    if (exchange_subscriber_) {
        std::cout << "[MARKET_SERVER_LIB] Starting exchange subscriber..." << std::endl;
        exchange_subscriber_->start();
    }
    
    std::cout << "[MARKET_SERVER_LIB] Started successfully" << std::endl;
}

void MarketServerLib::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (exchange_subscriber_) {
        std::cout << "[MARKET_SERVER_LIB] Stopping exchange subscriber..." << std::endl;
        exchange_subscriber_->stop();
    }
    
    std::cout << "[MARKET_SERVER_LIB] Stopped" << std::endl;
}

bool MarketServerLib::is_connected_to_exchange() const {
    return exchange_subscriber_ && exchange_subscriber_->is_connected();
}

void MarketServerLib::set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    std::cout << "[MARKET_SERVER_LIB] Setting custom WebSocket transport for testing" << std::endl;
    
    // Store the transport for later use when creating the exchange subscriber
    custom_transport_ = std::move(transport);
    
    // Recreate the exchange subscriber with the custom transport
    setup_exchange_subscriber();
}

void MarketServerLib::setup_exchange_subscriber() {
    std::cout << "[MARKET_SERVER_LIB] Setting up exchange subscriber for: " << exchange_name_ << std::endl;
    
    // Create exchange subscriber using factory
    exchange_subscriber_ = SubscriberFactory::create_subscriber(exchange_name_);
    if (!exchange_subscriber_) {
        std::cerr << "[MARKET_SERVER_LIB] Failed to create exchange subscriber for: " << exchange_name_ << std::endl;
        return;
    }
    
    // Set up callbacks
    exchange_subscriber_->set_orderbook_callback([this](const proto::OrderBookSnapshot& orderbook) {
        handle_orderbook_update(orderbook);
    });
    
    exchange_subscriber_->set_trade_callback([this](const proto::Trade& trade) {
        handle_trade_update(trade);
    });
    
    exchange_subscriber_->set_error_callback([this](const std::string& error) {
        handle_error(error);
    });
    
    // If we have a custom transport, inject it into the exchange subscriber
    if (custom_transport_) {
        std::cout << "[MARKET_SERVER_LIB] Injecting custom WebSocket transport" << std::endl;
        exchange_subscriber_->set_websocket_transport(std::move(custom_transport_));
    }
    
    std::cout << "[MARKET_SERVER_LIB] Exchange subscriber setup complete" << std::endl;
}

void MarketServerLib::handle_orderbook_update(const proto::OrderBookSnapshot& orderbook) {
    statistics_.orderbook_updates++;
    
    std::cout << "[MARKET_SERVER_LIB] Orderbook update: " << orderbook.symbol() 
              << " bids: " << orderbook.bids_size() << " asks: " << orderbook.asks_size() << std::endl;
    
    // Call the testing callback if set
    if (market_data_callback_) {
        market_data_callback_(orderbook);
    }
    
    // Publish to ZMQ
    publish_to_zmq("market_data", orderbook.SerializeAsString());
}

void MarketServerLib::handle_trade_update(const proto::Trade& trade) {
    statistics_.trade_updates++;
    
    std::cout << "[MARKET_SERVER_LIB] Trade update: " << trade.symbol() 
              << " @ " << trade.price() << " qty: " << trade.qty() << std::endl;
    
    // Call the testing callback if set
    if (trade_callback_) {
        trade_callback_(trade);
    }
    
    // Publish to ZMQ
    publish_to_zmq("trades", trade.SerializeAsString());
}

void MarketServerLib::handle_error(const std::string& error_message) {
    statistics_.connection_errors++;
    
    std::cerr << "[MARKET_SERVER_LIB] Error: " << error_message << std::endl;
    
    // Call the error callback if set
    if (error_callback_) {
        error_callback_(error_message);
    }
}

void MarketServerLib::publish_to_zmq(const std::string& topic, const std::string& message) {
    if (publisher_) {
        std::cout << "[MARKET_SERVER_LIB] Publishing to 0MQ topic: " << topic << " size: " << message.size() << " bytes" << std::endl;
        publisher_->publish(topic, message);
        statistics_.zmq_messages_sent++;
        std::cout << "[MARKET_SERVER_LIB] Published successfully" << std::endl;
    } else {
        std::cerr << "[MARKET_SERVER_LIB] No publisher available!" << std::endl;
    }
}

} // namespace market_server
