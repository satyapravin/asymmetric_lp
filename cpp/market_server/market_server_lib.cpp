#include "market_server_lib.hpp"
#include "../exchanges/subscriber_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include "../utils/logging/logger.hpp"
#include <thread>
#include <stdexcept>

namespace market_server {

MarketServerLib::MarketServerLib() 
    : running_(false), exchange_name_(""), symbol_("") {
}

MarketServerLib::~MarketServerLib() {
    stop();
}

bool MarketServerLib::initialize(const std::string& config_file) {
    logging::Logger logger("MARKET_SERVER_LIB");
    if (config_file.empty()) {
        logger.debug("Using default configuration");
    } else {
        logger.info("Loading configuration from: " + config_file);
        config_manager_ = std::make_unique<config::ProcessConfigManager>();
        if (!config_manager_->load_config(config_file)) {
            logger.error("Failed to load configuration from: " + config_file);
            return false;
        }
        
        // Load configuration values (only override if not explicitly set)
        if (exchange_name_.empty()) {
            exchange_name_ = config_manager_->get_string("market_server.exchange", "");
        }
        if (symbol_.empty()) {
            symbol_ = config_manager_->get_string("market_server.symbol", "");
        }
    }
    
    // Validate required configuration
    if (exchange_name_.empty()) {
        logger.error("ERROR: Exchange name not configured. Set it via set_exchange() or config file (market_server.exchange)");
        throw std::runtime_error("Exchange name not configured");
    }
    
    if (symbol_.empty()) {
        logger.error("ERROR: Symbol not configured. Set it via set_symbol() or config file (market_server.symbol)");
        throw std::runtime_error("Symbol not configured");
    }
    
    // Initialize ZMQ publisher
    publisher_ = std::make_shared<ZmqPublisher>("tcp://127.0.0.1:5555");
    
    // Setup exchange subscriber
    setup_exchange_subscriber();
    
    logger.info("Initialized with exchange: " + exchange_name_ + ", symbol: " + symbol_);
    return true;
}

void MarketServerLib::start() {
    logging::Logger logger("MARKET_SERVER_LIB");
    if (running_.load()) {
        logger.debug("Already running");
        return;
    }
    
    running_.store(true);
    
    if (exchange_subscriber_) {
        logger.info("Starting exchange subscriber...");
        
        // Connect and subscribe to orderbook
        if (exchange_subscriber_->connect()) {
            logger.info("Connected to exchange");
            
            // Subscribe to orderbook for the configured symbol
            if (!symbol_.empty()) {
                exchange_subscriber_->subscribe_orderbook(symbol_, 20, 100);
                logger.info("Subscribed to orderbook for: " + symbol_);
            }
        } else {
            logger.error("Failed to connect to exchange");
        }
        
        exchange_subscriber_->start();
    }
    
    logger.info("Started successfully");
}

void MarketServerLib::stop() {
    logging::Logger logger("MARKET_SERVER_LIB");
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (exchange_subscriber_) {
        logger.info("Stopping exchange subscriber...");
        exchange_subscriber_->stop();
    }
    
    logger.info("Stopped");
}

bool MarketServerLib::is_connected_to_exchange() const {
    return exchange_subscriber_ && exchange_subscriber_->is_connected();
}

void MarketServerLib::set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    logging::Logger logger("MARKET_SERVER_LIB");
    logger.debug("Setting custom WebSocket transport for testing");
    
    // Store the transport for later use when creating the exchange subscriber
    custom_transport_ = std::move(transport);
    
    // Recreate the exchange subscriber with the custom transport
    setup_exchange_subscriber();
}

void MarketServerLib::setup_exchange_subscriber() {
    logging::Logger logger("MARKET_SERVER_LIB");
    logger.info("Setting up exchange subscriber for: " + exchange_name_);
    
    // Create exchange subscriber using factory
    exchange_subscriber_ = SubscriberFactory::create_subscriber(exchange_name_);
    if (!exchange_subscriber_) {
        logger.error("Failed to create exchange subscriber for: " + exchange_name_);
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
        logger.debug("Injecting custom WebSocket transport");
        exchange_subscriber_->set_websocket_transport(std::move(custom_transport_));
    }
    
    logger.debug("Exchange subscriber setup complete");
}

void MarketServerLib::handle_orderbook_update(const proto::OrderBookSnapshot& orderbook) {
    statistics_.orderbook_updates++;
    
    logging::Logger logger("MARKET_SERVER_LIB");
    logger.debug("Orderbook update: " + orderbook.symbol() + 
                " bids: " + std::to_string(orderbook.bids_size()) + 
                " asks: " + std::to_string(orderbook.asks_size()));
    
    // Call the testing callback if set
    if (market_data_callback_) {
        market_data_callback_(orderbook);
    }
    
    // Publish to ZMQ
    publish_to_zmq("market_data", orderbook.SerializeAsString());
}

void MarketServerLib::handle_trade_update(const proto::Trade& trade) {
    statistics_.trade_updates++;
    
    logging::Logger logger("MARKET_SERVER_LIB");
    std::stringstream ss;
    ss << "Trade update: " << trade.symbol() << " @ " << trade.price() << " qty: " << trade.qty();
    logger.debug(ss.str());
    
    // Call the testing callback if set
    if (trade_callback_) {
        trade_callback_(trade);
    }
    
    // Publish to ZMQ
    publish_to_zmq("trades", trade.SerializeAsString());
}

void MarketServerLib::handle_error(const std::string& error_message) {
    statistics_.connection_errors++;
    
    logging::Logger logger("MARKET_SERVER_LIB");
    logger.error("Error: " + error_message);
    
    // Call the error callback if set
    if (error_callback_) {
        error_callback_(error_message);
    }
}

void MarketServerLib::publish_to_zmq(const std::string& topic, const std::string& message) {
    logging::Logger logger("MARKET_SERVER_LIB");
    if (publisher_) {
        logger.debug("Publishing to 0MQ topic: " + topic + " size: " + std::to_string(message.size()) + " bytes");
        bool success = publisher_->publish(topic, message);
        if (success) {
            statistics_.zmq_messages_sent++;
        } else {
            statistics_.zmq_messages_dropped++;
            // Warning already logged by ZmqPublisher
        }
    } else {
        logger.error("No publisher available!");
    }
}

} // namespace market_server
