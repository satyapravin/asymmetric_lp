#include "position_server_service.hpp"
#include "../utils/logging/log_helper.hpp"

namespace position_server {

PositionServerService::PositionServerService() 
    : app_service::AppService("PositionServer") {
}

bool PositionServerService::configure_service() {
    // Get configuration values
    exchange_ = get_config_manager()->get_string("position.exchange", "BINANCE");
    zmq_publish_endpoint_ = get_config_manager()->get_string("zmq.publish_endpoint", "tcp://*:5556");
    
    LOG_INFO_COMP("POSITION_SERVER", "Exchange: " + exchange_);
    LOG_INFO_COMP("POSITION_SERVER", "ZMQ publish endpoint: " + zmq_publish_endpoint_);
    
    // Initialize ZMQ publisher
    publisher_ = std::make_shared<ZmqPublisher>(zmq_publish_endpoint_);
    if (!publisher_->bind()) {
        LOG_ERROR_COMP("POSITION_SERVER", "Failed to bind ZMQ publisher");
        return false;
    }
    
    // Initialize position server library
    position_server_lib_ = std::make_unique<PositionServerLib>();
    position_server_lib_->set_exchange(exchange_);
    position_server_lib_->set_zmq_publisher(publisher_);
    
    // Initialize the library
    if (!position_server_lib_->initialize(get_config_file())) {
        LOG_ERROR_COMP("POSITION_SERVER", "Failed to initialize position server library");
        return false;
    }
    
    return true;
}

bool PositionServerService::start_service() {
    if (!position_server_lib_) {
        return false;
    }
    
    position_server_lib_->start();
    
    LOG_INFO_COMP("POSITION_SERVER", "Processing position and balance updates for " + exchange_);
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
    
    std::string stats_msg = get_service_name() + " - " +
                            "Position updates: " + std::to_string(stats.position_updates.load()) +
                            ", Balance updates: " + std::to_string(stats.balance_updates.load()) +
                            ", ZMQ messages sent: " + std::to_string(stats.zmq_messages_sent.load()) +
                            ", ZMQ messages dropped: " + std::to_string(stats.zmq_messages_dropped.load()) +
                            ", Connection errors: " + std::to_string(stats.connection_errors.load()) +
                            ", Uptime: " + std::to_string(app_stats.uptime_seconds.load()) + "s";
    LOG_INFO_COMP("STATS", stats_msg);
}

} // namespace position_server
