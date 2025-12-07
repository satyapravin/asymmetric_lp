#include "market_server_service.hpp"
#include "../utils/logging/log_helper.hpp"

namespace market_server {

MarketServerService::MarketServerService() 
    : app_service::AppService("MarketServer") {
}

bool MarketServerService::configure_service() {
    // Get configuration values
    exchange_ = get_config_manager()->get_string("market.exchange", "BINANCE");
    symbol_ = get_config_manager()->get_string("market.symbol", "BTCUSDT");
    zmq_publish_endpoint_ = get_config_manager()->get_string("zmq.publish_endpoint", "tcp://*:5555");
    
    LOG_INFO_COMP("MARKET_SERVER", "Exchange: " + exchange_);
    LOG_INFO_COMP("MARKET_SERVER", "Symbol: " + symbol_);
    LOG_INFO_COMP("MARKET_SERVER", "ZMQ publish endpoint: " + zmq_publish_endpoint_);
    
    // Initialize ZMQ publisher with CONFLATE enabled for market data
    // CONFLATE keeps only the latest message - perfect for orderbook snapshots
    // This provides true "fire-and-forget" behavior where only current state matters
    publisher_ = std::make_shared<ZmqPublisher>(zmq_publish_endpoint_, 1000, true);
    if (!publisher_->bind()) {
        LOG_ERROR_COMP("MARKET_SERVER", "Failed to bind ZMQ publisher");
        return false;
    }
    
    // Initialize market server library
    market_server_lib_ = std::make_unique<MarketServerLib>();
    market_server_lib_->set_exchange(exchange_);
    market_server_lib_->set_symbol(symbol_);
    market_server_lib_->set_zmq_publisher(publisher_);
    
    // Initialize the library
    if (!market_server_lib_->initialize(get_config_file())) {
        LOG_ERROR_COMP("MARKET_SERVER", "Failed to initialize market server library");
        return false;
    }
    
    return true;
}

bool MarketServerService::start_service() {
    if (!market_server_lib_) {
        return false;
    }
    
    market_server_lib_->start();
    
    LOG_INFO_COMP("MARKET_SERVER", "Processing market data for " + exchange_ + ":" + symbol_);
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
    
    std::string stats_msg = get_service_name() + " - " +
                            "Orderbook updates: " + std::to_string(stats.orderbook_updates.load()) +
                            ", Trade updates: " + std::to_string(stats.trade_updates.load()) +
                            ", ZMQ messages sent: " + std::to_string(stats.zmq_messages_sent.load()) +
                            ", ZMQ messages dropped: " + std::to_string(stats.zmq_messages_dropped.load()) +
                            ", Connection errors: " + std::to_string(stats.connection_errors.load()) +
                            ", Uptime: " + std::to_string(app_stats.uptime_seconds.load()) + "s";
    LOG_INFO_COMP("STATS", stats_msg);
}

} // namespace market_server
