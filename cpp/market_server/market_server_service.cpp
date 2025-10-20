#include "market_server_service.hpp"
#include <iostream>

namespace market_server {

MarketServerService::MarketServerService() 
    : app_service::AppService("MarketServer") {
}

bool MarketServerService::configure_service() {
    // Get configuration values
    exchange_ = get_config_manager()->get_string("market.exchange", "BINANCE");
    symbol_ = get_config_manager()->get_string("market.symbol", "BTCUSDT");
    zmq_publish_endpoint_ = get_config_manager()->get_string("zmq.publish_endpoint", "tcp://*:5555");
    
    std::cout << "[MARKET_SERVER] Exchange: " << exchange_ << std::endl;
    std::cout << "[MARKET_SERVER] Symbol: " << symbol_ << std::endl;
    std::cout << "[MARKET_SERVER] ZMQ publish endpoint: " << zmq_publish_endpoint_ << std::endl;
    
    // Initialize ZMQ publisher
    publisher_ = std::make_shared<ZmqPublisher>(zmq_publish_endpoint_);
    if (!publisher_->bind()) {
        std::cerr << "[MARKET_SERVER] Failed to bind ZMQ publisher" << std::endl;
        return false;
    }
    
    // Initialize market server library
    market_server_lib_ = std::make_unique<MarketServerLib>();
    market_server_lib_->set_exchange(exchange_);
    market_server_lib_->set_symbol(symbol_);
    market_server_lib_->set_zmq_publisher(publisher_);
    
    // Initialize the library
    if (!market_server_lib_->initialize(get_config_file())) {
        std::cerr << "[MARKET_SERVER] Failed to initialize market server library" << std::endl;
        return false;
    }
    
    return true;
}

bool MarketServerService::start_service() {
    if (!market_server_lib_) {
        return false;
    }
    
    market_server_lib_->start();
    
    std::cout << "[MARKET_SERVER] Processing market data for " << exchange_ << ":" << symbol_ << std::endl;
    return true;
}

void MarketServerService::stop_service() {
    if (market_server_lib_) {
        market_server_lib_->stop();
    }
}

void MarketServerService::print_service_stats() {
    if (!market_server_lib_) {
        return;
    }
    
    const auto& stats = market_server_lib_->get_statistics();
    const auto& app_stats = get_statistics();
    
    std::cout << "[STATS] " << get_service_name() << " - "
              << "Orderbook updates: " << stats.orderbook_updates.load()
              << ", Trade updates: " << stats.trade_updates.load()
              << ", ZMQ messages sent: " << stats.zmq_messages_sent.load()
              << ", Connection errors: " << stats.connection_errors.load()
              << ", Uptime: " << app_stats.uptime_seconds.load() << "s" << std::endl;
}

} // namespace market_server
