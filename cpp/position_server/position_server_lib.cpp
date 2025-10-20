#include "position_server_lib.hpp"
#include "../exchanges/pms_factory.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/config/process_config_manager.hpp"
#include <iostream>
#include <thread>

namespace position_server {

PositionServerLib::PositionServerLib() 
    : running_(false), exchange_name_("binance") {
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
        
        // Load configuration values
        exchange_name_ = config_manager_->get_string("position_server.exchange", "binance");
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
        exchange_pms_->start();
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
        exchange_pms_->stop();
    }
    
    std::cout << "[POSITION_SERVER_LIB] Stopped" << std::endl;
}

bool PositionServerLib::is_connected_to_exchange() const {
    return exchange_pms_ && exchange_pms_->is_connected();
}

void PositionServerLib::setup_exchange_pms() {
    std::cout << "[POSITION_SERVER_LIB] Setting up exchange PMS for: " << exchange_name_ << std::endl;
    
    // Create exchange PMS using factory
    exchange_pms_ = PMSFactory::create(exchange_name_);
    if (!exchange_pms_) {
        std::cerr << "[POSITION_SERVER_LIB] Failed to create exchange PMS for: " << exchange_name_ << std::endl;
        return;
    }
    
    // Set up callbacks
    exchange_pms_->set_position_update_callback([this](const proto::PositionUpdate& position) {
        handle_position_update(position);
    });
    
    exchange_pms_->set_balance_update_callback([this](const proto::AccountBalanceUpdate& balance) {
        handle_balance_update(balance);
    });
    
    exchange_pms_->set_error_callback([this](const std::string& error) {
        handle_error(error);
    });
    
    std::cout << "[POSITION_SERVER_LIB] Exchange PMS setup complete" << std::endl;
}

void PositionServerLib::handle_position_update(const proto::PositionUpdate& position) {
    statistics_.position_updates++;
    
    std::cout << "[POSITION_SERVER_LIB] Position update: " << position.symbol() 
              << " size: " << position.size() << " unrealized_pnl: " << position.unrealized_pnl() << std::endl;
    
    // Call the testing callback if set
    if (position_callback_) {
        position_callback_(position);
    }
    
    // Publish to ZMQ
    publish_to_zmq("position_updates", position.SerializeAsString());
}

void PositionServerLib::handle_balance_update(const proto::AccountBalanceUpdate& balance) {
    statistics_.balance_updates++;
    
    std::cout << "[POSITION_SERVER_LIB] Balance update: " << balance.exchange() 
              << " balances: " << balance.balances_size() << std::endl;
    
    // Call the testing callback if set
    if (balance_callback_) {
        balance_callback_(balance);
    }
    
    // Publish to ZMQ
    publish_to_zmq("balance_updates", balance.SerializeAsString());
}

void PositionServerLib::handle_error(const std::string& error_message) {
    statistics_.connection_errors++;
    
    std::cerr << "[POSITION_SERVER_LIB] Error: " << error_message << std::endl;
    
    // Call the error callback if set
    if (error_callback_) {
        error_callback_(error_message);
    }
}

void PositionServerLib::publish_to_zmq(const std::string& topic, const std::string& message) {
    if (publisher_) {
        publisher_->publish(topic, message);
        statistics_.zmq_messages_sent++;
    }
}

} // namespace position_server
