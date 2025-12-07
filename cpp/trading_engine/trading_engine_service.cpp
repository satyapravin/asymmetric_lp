#include "trading_engine_service.hpp"
#include "trading_engine_lib.hpp"
#include "../utils/logging/log_helper.hpp"
#include <signal.h>
#include <unistd.h>

namespace trading_engine {

TradingEngineService::TradingEngineService() : AppService("trading_engine") {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Initializing Trading Engine Service");
    
    // Initialize the trading engine library
    trading_engine_lib_ = std::make_unique<TradingEngineLib>();
    
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Service initialized");
}

TradingEngineService::~TradingEngineService() {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Destroying Trading Engine Service");
    stop();
}

bool TradingEngineService::configure_service() {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Configuring service");
    
    if (!trading_engine_lib_) {
        LOG_ERROR_COMP("TRADING_ENGINE_SERVICE", "Trading engine library not initialized");
        return false;
    }
    
    // Initialize the trading engine library
    if (!trading_engine_lib_->initialize(get_config_file())) {
        LOG_ERROR_COMP("TRADING_ENGINE_SERVICE", "Failed to initialize trading engine library");
        return false;
    }
    
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Service configuration complete");
    return true;
}

bool TradingEngineService::start_service() {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Starting service");
    
    if (!trading_engine_lib_) {
        LOG_ERROR_COMP("TRADING_ENGINE_SERVICE", "Trading engine library not initialized");
        return false;
    }
    
    // Start the trading engine library
    trading_engine_lib_->start();
    
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Service started");
    return true;
}

void TradingEngineService::stop_service() {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Stopping service");
    
    if (trading_engine_lib_) {
        trading_engine_lib_->stop();
    }
    
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Service stopped");
}

void TradingEngineService::print_service_stats() {
    LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "Service Statistics:");
    
    if (trading_engine_lib_) {
        const auto& stats = trading_engine_lib_->get_statistics();
        
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Received: " + std::to_string(stats.orders_received.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Sent to Exchange: " + std::to_string(stats.orders_sent_to_exchange.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Acknowledged: " + std::to_string(stats.orders_acked.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Filled: " + std::to_string(stats.orders_filled.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Cancelled: " + std::to_string(stats.orders_cancelled.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Orders Rejected: " + std::to_string(stats.orders_rejected.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Trade Executions: " + std::to_string(stats.trade_executions.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  ZMQ Messages Received: " + std::to_string(stats.zmq_messages_received.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  ZMQ Messages Sent: " + std::to_string(stats.zmq_messages_sent.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  ZMQ Messages Dropped: " + std::to_string(stats.zmq_messages_dropped.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Connection Errors: " + std::to_string(stats.connection_errors.load()));
        LOG_INFO_COMP("TRADING_ENGINE_SERVICE", "  Parse Errors: " + std::to_string(stats.parse_errors.load()));
    } else {
        LOG_WARN_COMP("TRADING_ENGINE_SERVICE", "  Trading engine library not initialized");
    }
}

} // namespace trading_engine
