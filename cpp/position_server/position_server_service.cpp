#include "position_server_service.hpp"
#include <iostream>

namespace position_server {

PositionServerService::PositionServerService() 
    : app_service::AppService("PositionServer") {
}

bool PositionServerService::configure_service() {
    // Get configuration values
    exchange_ = get_config_manager()->get_string("position.exchange", "BINANCE");
    zmq_publish_endpoint_ = get_config_manager()->get_string("zmq.publish_endpoint", "tcp://*:5556");
    
    std::cout << "[POSITION_SERVER] Exchange: " << exchange_ << std::endl;
    std::cout << "[POSITION_SERVER] ZMQ publish endpoint: " << zmq_publish_endpoint_ << std::endl;
    
    // Initialize ZMQ publisher
    publisher_ = std::make_shared<ZmqPublisher>(zmq_publish_endpoint_);
    if (!publisher_->bind()) {
        std::cerr << "[POSITION_SERVER] Failed to bind ZMQ publisher" << std::endl;
        return false;
    }
    
    // Initialize position server library
    position_server_lib_ = std::make_unique<PositionServerLib>();
    position_server_lib_->set_exchange(exchange_);
    position_server_lib_->set_zmq_publisher(publisher_);
    
    // Initialize the library
    if (!position_server_lib_->initialize(get_config_file())) {
        std::cerr << "[POSITION_SERVER] Failed to initialize position server library" << std::endl;
        return false;
    }
    
    return true;
}

bool PositionServerService::start_service() {
    if (!position_server_lib_) {
        return false;
    }
    
    position_server_lib_->start();
    
    std::cout << "[POSITION_SERVER] Processing position and balance updates for " << exchange_ << std::endl;
    return true;
}

void PositionServerService::stop_service() {
    if (position_server_lib_) {
        position_server_lib_->stop();
    }
}

void PositionServerService::print_service_stats() {
    if (!position_server_lib_) {
        return;
    }
    
    const auto& stats = position_server_lib_->get_statistics();
    const auto& app_stats = get_statistics();
    
    std::cout << "[STATS] " << get_service_name() << " - "
              << "Position updates: " << stats.position_updates.load()
              << ", Balance updates: " << stats.balance_updates.load()
              << ", ZMQ messages sent: " << stats.zmq_messages_sent.load()
              << ", Connection errors: " << stats.connection_errors.load()
              << ", Uptime: " << app_stats.uptime_seconds.load() << "s" << std::endl;
}

} // namespace position_server
