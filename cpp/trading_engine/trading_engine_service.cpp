#include "trading_engine_service.hpp"
#include "trading_engine_lib.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>

namespace trading_engine {

TradingEngineService::TradingEngineService() : AppService("trading_engine") {
    std::cout << "[TRADING_ENGINE_SERVICE] Initializing Trading Engine Service" << std::endl;
    
    // Initialize the trading engine library
    trading_engine_lib_ = std::make_unique<TradingEngineLib>();
    
    std::cout << "[TRADING_ENGINE_SERVICE] Service initialized" << std::endl;
}

TradingEngineService::~TradingEngineService() {
    std::cout << "[TRADING_ENGINE_SERVICE] Destroying Trading Engine Service" << std::endl;
    stop();
}

bool TradingEngineService::configure_service() {
    std::cout << "[TRADING_ENGINE_SERVICE] Configuring service" << std::endl;
    
    if (!trading_engine_lib_) {
        std::cerr << "[TRADING_ENGINE_SERVICE] Trading engine library not initialized" << std::endl;
        return false;
    }
    
    // Initialize the trading engine library
    if (!trading_engine_lib_->initialize(get_config_file())) {
        std::cerr << "[TRADING_ENGINE_SERVICE] Failed to initialize trading engine library" << std::endl;
        return false;
    }
    
    std::cout << "[TRADING_ENGINE_SERVICE] Service configuration complete" << std::endl;
    return true;
}

bool TradingEngineService::start_service() {
    std::cout << "[TRADING_ENGINE_SERVICE] Starting service" << std::endl;
    
    if (!trading_engine_lib_) {
        std::cerr << "[TRADING_ENGINE_SERVICE] Trading engine library not initialized" << std::endl;
        return false;
    }
    
    // Start the trading engine library
    trading_engine_lib_->start();
    
    std::cout << "[TRADING_ENGINE_SERVICE] Service started" << std::endl;
    return true;
}

void TradingEngineService::stop_service() {
    std::cout << "[TRADING_ENGINE_SERVICE] Stopping service" << std::endl;
    
    if (trading_engine_lib_) {
        trading_engine_lib_->stop();
    }
    
    std::cout << "[TRADING_ENGINE_SERVICE] Service stopped" << std::endl;
}

void TradingEngineService::print_service_stats() {
    std::cout << "[TRADING_ENGINE_SERVICE] Service Statistics:" << std::endl;
    
    if (trading_engine_lib_) {
        const auto& stats = trading_engine_lib_->get_statistics();
        
        std::cout << "  Orders Received: " << stats.orders_received.load() << std::endl;
        std::cout << "  Orders Sent to Exchange: " << stats.orders_sent_to_exchange.load() << std::endl;
        std::cout << "  Orders Acknowledged: " << stats.orders_acked.load() << std::endl;
        std::cout << "  Orders Filled: " << stats.orders_filled.load() << std::endl;
        std::cout << "  Orders Cancelled: " << stats.orders_cancelled.load() << std::endl;
        std::cout << "  Orders Rejected: " << stats.orders_rejected.load() << std::endl;
        std::cout << "  Trade Executions: " << stats.trade_executions.load() << std::endl;
        std::cout << "  ZMQ Messages Received: " << stats.zmq_messages_received.load() << std::endl;
        std::cout << "  ZMQ Messages Sent: " << stats.zmq_messages_sent.load() << std::endl;
        std::cout << "  Connection Errors: " << stats.connection_errors.load() << std::endl;
        std::cout << "  Parse Errors: " << stats.parse_errors.load() << std::endl;
    } else {
        std::cout << "  Trading engine library not initialized" << std::endl;
    }
}

} // namespace trading_engine
