#include "trader_lib.hpp"
#include <iostream>

namespace trader {

TraderLib::TraderLib() : running_(false) {
    std::cout << "[TRADER_LIB] Initializing Trader Library" << std::endl;
}

TraderLib::~TraderLib() {
    stop();
    std::cout << "[TRADER_LIB] Destroying Trader Library" << std::endl;
}

bool TraderLib::initialize(const std::string& config_file) {
    std::cout << "[TRADER_LIB] Initializing with config: " << config_file << std::endl;
    
    // Create strategy container
    strategy_container_ = std::make_unique<StrategyContainer>();
    
    return true;
}

void TraderLib::start() {
    std::cout << "[TRADER_LIB] Starting Trader Library" << std::endl;
    
    if (running_.load()) {
        std::cout << "[TRADER_LIB] Already running" << std::endl;
        return;
    }
    
    // Start strategy container
    if (strategy_container_) {
        strategy_container_->start();
    }
    
    running_.store(true);
    std::cout << "[TRADER_LIB] Started successfully" << std::endl;
}

void TraderLib::stop() {
    std::cout << "[TRADER_LIB] Stopping Trader Library" << std::endl;
    
    if (!running_.load()) {
        std::cout << "[TRADER_LIB] Already stopped" << std::endl;
        return;
    }
    
    // Stop strategy container
    if (strategy_container_) {
        strategy_container_->stop();
    }
    
    // Stop ZMQ adapters
    if (mds_adapter_) {
        std::cout << "[TRADER_LIB] Stopping MDS adapter" << std::endl;
        mds_adapter_->stop();
    }
    if (oms_adapter_) {
        // TODO: Add stop() method to OMS adapter
    }
    if (pms_adapter_) {
        // TODO: Add stop() method to PMS adapter
    }
    
    running_.store(false);
    std::cout << "[TRADER_LIB] Stopped successfully" << std::endl;
}

void TraderLib::set_strategy(std::shared_ptr<AbstractStrategy> strategy) {
    std::cout << "[TRADER_LIB] Setting strategy" << std::endl;
    
    if (!strategy_container_) {
        std::cerr << "[TRADER_LIB] Strategy container not initialized!" << std::endl;
        return;
    }
    
    strategy_container_->set_strategy(strategy);
    
    // Set up MDS adapter callback to forward market data to strategy container
    if (mds_adapter_) {
        std::cout << "[TRADER_LIB] Setting up MDS adapter callback" << std::endl;
        mds_adapter_->on_snapshot = [this](const proto::OrderBookSnapshot& orderbook) {
            std::cout << "[TRADER_LIB] MDS adapter received orderbook: " << orderbook.symbol() 
                      << " bids: " << orderbook.bids_size() << " asks: " << orderbook.asks_size() << std::endl;
            
            // Forward proto::OrderBookSnapshot directly to strategy container
            strategy_container_->on_market_data(orderbook);
        };
    }
}

std::shared_ptr<AbstractStrategy> TraderLib::get_strategy() const {
    if (!strategy_container_) {
        return nullptr;
    }
    
    return strategy_container_->get_strategy();
}

} // namespace trader
