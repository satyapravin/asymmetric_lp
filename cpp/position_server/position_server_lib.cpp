#include "position_server_lib.hpp"
#include "../exchanges/pms_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include "../utils/logging/logger.hpp"
#include <thread>

namespace position_server {

PositionServerLib::PositionServerLib() 
    : running_(false), exchange_name_("") {
}

PositionServerLib::~PositionServerLib() {
    stop();
}

bool PositionServerLib::initialize(const std::string& config_file) {
    logging::Logger logger("POSITION_SERVER_LIB");
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
            exchange_name_ = config_manager_->get_string("position_server.exchange", "");
        }
    }
    
    // Validate required configuration
    if (exchange_name_.empty()) {
        logger.error("ERROR: Exchange name not configured. Set it via set_exchange() or config file (position_server.exchange)");
        throw std::runtime_error("Exchange name not configured");
    }
    
    // Setup exchange PMS
    setup_exchange_pms();
    
    logger.info("Initialized with exchange: " + exchange_name_);
    return true;
}

void PositionServerLib::start() {
    logging::Logger logger("POSITION_SERVER_LIB");
    if (running_.load()) {
        logger.debug("Already running");
        return;
    }
    
    running_.store(true);
    
    if (exchange_pms_) {
        logger.info("Starting exchange PMS...");
        exchange_pms_->connect();
    }
    
    logger.info("Started successfully");
}

void PositionServerLib::stop() {
    logging::Logger logger("POSITION_SERVER_LIB");
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (exchange_pms_) {
        logger.info("Stopping exchange PMS...");
        exchange_pms_->disconnect();
    }
    
    logger.info("Stopped");
}

bool PositionServerLib::is_connected_to_exchange() const {
    return exchange_pms_ && exchange_pms_->is_connected();
}

void PositionServerLib::setup_exchange_pms() {
    logging::Logger logger("POSITION_SERVER_LIB");
    if (exchange_name_.empty()) {
        logger.error("Cannot setup PMS: exchange name not set");
        return;
    }
    
    logger.info("Setting up exchange PMS for: " + exchange_name_);
    
    try {
        // Create exchange PMS using factory
        exchange_pms_ = PMSFactory::create_pms(exchange_name_);
        if (!exchange_pms_) {
            logger.error("Failed to create exchange PMS for: " + exchange_name_);
            return;
        }
    } catch (const std::exception& e) {
        logger.error("Exception creating PMS: " + std::string(e.what()));
        throw;
    }
    
    // Set up callbacks
    exchange_pms_->set_position_update_callback([this](const proto::PositionUpdate& position) {
        handle_position_update(position);
    });
    
    exchange_pms_->set_account_balance_update_callback([this](const proto::AccountBalanceUpdate& balance) {
        handle_balance_update(balance);
    });
    
    logger.debug("Exchange PMS setup complete");
}

void PositionServerLib::handle_position_update(const proto::PositionUpdate& position) {
    statistics_.position_updates++;
    
    logging::Logger logger("POSITION_SERVER_LIB");
    std::stringstream ss;
    ss << "Position update: " << position.symbol() << " qty: " << position.qty() << " avg_price: " << position.avg_price();
    logger.debug(ss.str());
    
    // Publish to ZMQ
    publish_to_zmq("position_updates", position.SerializeAsString());
}

void PositionServerLib::handle_balance_update(const proto::AccountBalanceUpdate& balance) {
    statistics_.balance_updates++;
    
    logging::Logger logger("POSITION_SERVER_LIB");
    logger.debug("Balance update: balances: " + std::to_string(balance.balances_size()));
    
    // Publish to ZMQ
    publish_to_zmq("balance_updates", balance.SerializeAsString());
}

void PositionServerLib::handle_error(const std::string& error_message) {
    statistics_.connection_errors++;
    
    logging::Logger logger("POSITION_SERVER_LIB");
    logger.error("Error: " + error_message);
}

void PositionServerLib::publish_to_zmq(const std::string& topic, const std::string& message) {
    logging::Logger logger("POSITION_SERVER_LIB");
    if (publisher_) {
        bool success = publisher_->publish(topic, message);
        if (success) {
            statistics_.zmq_messages_sent++;
            logger.debug("Published to ZMQ topic: " + topic + " size: " + std::to_string(message.size()) + " bytes");
        } else {
            statistics_.zmq_messages_dropped++;
            // Warning already logged by ZmqPublisher
        }
    } else {
        logger.error("No ZMQ publisher available!");
    }
}

void PositionServerLib::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    if (exchange_pms_) {
        exchange_pms_->set_websocket_transport(transport);
        logging::Logger logger("POSITION_SERVER_LIB");
        logger.debug("WebSocket transport injected for testing");
    }
}

} // namespace position_server
