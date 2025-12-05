#include "position_server_lib.hpp"
#include "../exchanges/pms_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include <iostream>
#include <thread>

namespace position_server {

PositionServerLib::PositionServerLib() 
    : running_(false), exchange_name_("") {
}

PositionServerLib::~PositionServerLib() {
    stop();
}

bool PositionServerLib::initialize(const std::string& config_file) {
    if (config_file.empty()) {
        std::cout << "[POSITION_SERVER_LIB] Using default configuration" << std::endl;
    } else {
        std::cout << "[POSITION_SERVER_LIB] Loading configuration from: " << config_file << std::endl;
        config_manager_ = std::make_unique<config::ProcessConfigManager>();
        if (!config_manager_->load_config(config_file)) {
            std::cerr << "[POSITION_SERVER_LIB] Failed to load configuration from: " << config_file << std::endl;
            return false;
        }
        
        // Load configuration values (only override if not explicitly set)
        if (exchange_name_.empty()) {
            exchange_name_ = config_manager_->get_string("position_server.exchange", "");
        }
    }
    
    // Validate required configuration
    if (exchange_name_.empty()) {
        std::cerr << "[POSITION_SERVER_LIB] ERROR: Exchange name not configured. "
                  << "Set it via set_exchange() or config file (position_server.exchange)" << std::endl;
        throw std::runtime_error("Exchange name not configured");
    }
    
    // Setup exchange PMS
    setup_exchange_pms();
    
    std::cout << "[POSITION_SERVER_LIB] Initialized with exchange: " << exchange_name_ << std::endl;
    return true;
}

void PositionServerLib::start() {
    if (running_.load()) {
        std::cout << "[POSITION_SERVER_LIB] Already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    if (exchange_pms_) {
        std::cout << "[POSITION_SERVER_LIB] Starting exchange PMS..." << std::endl;
        exchange_pms_->connect();
    }
    
    std::cout << "[POSITION_SERVER_LIB] Started successfully" << std::endl;
}

void PositionServerLib::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (exchange_pms_) {
        std::cout << "[POSITION_SERVER_LIB] Stopping exchange PMS..." << std::endl;
        exchange_pms_->disconnect();
    }
    
    std::cout << "[POSITION_SERVER_LIB] Stopped" << std::endl;
}

bool PositionServerLib::is_connected_to_exchange() const {
    return exchange_pms_ && exchange_pms_->is_connected();
}

void PositionServerLib::setup_exchange_pms() {
    if (exchange_name_.empty()) {
        std::cerr << "[POSITION_SERVER_LIB] Cannot setup PMS: exchange name not set" << std::endl;
        return;
    }
    
    std::cout << "[POSITION_SERVER_LIB] Setting up exchange PMS for: " << exchange_name_ << std::endl;
    
    try {
        // Create exchange PMS using factory
        exchange_pms_ = PMSFactory::create_pms(exchange_name_);
        if (!exchange_pms_) {
            std::cerr << "[POSITION_SERVER_LIB] Failed to create exchange PMS for: " << exchange_name_ << std::endl;
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "[POSITION_SERVER_LIB] Exception creating PMS: " << e.what() << std::endl;
        throw;
    }
    
    // Set up callbacks
    exchange_pms_->set_position_update_callback([this](const proto::PositionUpdate& position) {
        handle_position_update(position);
    });
    
    exchange_pms_->set_account_balance_update_callback([this](const proto::AccountBalanceUpdate& balance) {
        handle_balance_update(balance);
    });
    
    std::cout << "[POSITION_SERVER_LIB] Exchange PMS setup complete" << std::endl;
}

void PositionServerLib::handle_position_update(const proto::PositionUpdate& position) {
    statistics_.position_updates++;
    
    std::cout << "[POSITION_SERVER_LIB] Position update: " << position.symbol() 
              << " qty: " << position.qty() << " avg_price: " << position.avg_price() << std::endl;
    
    // Publish to ZMQ
    publish_to_zmq("position_updates", position.SerializeAsString());
}

void PositionServerLib::handle_balance_update(const proto::AccountBalanceUpdate& balance) {
    statistics_.balance_updates++;
    
    std::cout << "[POSITION_SERVER_LIB] Balance update: " 
              << " balances: " << balance.balances_size() << std::endl;
    
    // Publish to ZMQ
    publish_to_zmq("balance_updates", balance.SerializeAsString());
}

void PositionServerLib::handle_error(const std::string& error_message) {
    statistics_.connection_errors++;
    
    std::cerr << "[POSITION_SERVER_LIB] Error: " << error_message << std::endl;
}

void PositionServerLib::publish_to_zmq(const std::string& topic, const std::string& message) {
    if (publisher_) {
        publisher_->publish(topic, message);
        statistics_.zmq_messages_sent++;
        std::cout << "[POSITION_SERVER_LIB] Published to ZMQ topic: " << topic << " size: " << message.size() << " bytes" << std::endl;
    } else {
        std::cout << "[POSITION_SERVER_LIB] No ZMQ publisher available!" << std::endl;
    }
}

void PositionServerLib::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    if (exchange_pms_) {
        exchange_pms_->set_websocket_transport(transport);
        std::cout << "[POSITION_SERVER_LIB] WebSocket transport injected for testing" << std::endl;
    }
}

} // namespace position_server
