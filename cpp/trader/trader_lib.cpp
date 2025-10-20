#include "trader_lib.hpp"
#include <iostream>

namespace trader {

TraderLib::TraderLib() : running_(false), oms_event_running_(false) {
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
    
    // Create ZMQ adapters based on configuration
    // For now, use default endpoints - these should come from config in production
    std::string mds_endpoint = "tcp://127.0.0.1:5555";
    std::string pms_endpoint = "tcp://127.0.0.1:5556";
    std::string oms_publish_endpoint = "tcp://127.0.0.1:5557";
    std::string oms_subscribe_endpoint = "tcp://127.0.0.1:5558";
    
    // Create MDS adapter
    mds_adapter_ = std::make_shared<ZmqMDSAdapter>(mds_endpoint, "market_data", exchange_);
    std::cout << "[TRADER_LIB] Created MDS adapter for endpoint: " << mds_endpoint << std::endl;
    
    // Create PMS adapter
    pms_adapter_ = std::make_shared<ZmqPMSAdapter>(pms_endpoint, "position_updates");
    std::cout << "[TRADER_LIB] Created PMS adapter for endpoint: " << pms_endpoint << std::endl;
    
    // Create OMS adapter
    oms_adapter_ = std::make_shared<ZmqOMSAdapter>(oms_publish_endpoint, "orders", oms_subscribe_endpoint, "order_events");
    std::cout << "[TRADER_LIB] Created OMS adapter for endpoints: " << oms_publish_endpoint << " / " << oms_subscribe_endpoint << std::endl;
    
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
    
    // Start OMS adapter polling
    if (oms_adapter_) {
        std::cout << "[TRADER_LIB] Starting OMS adapter polling" << std::endl;
        oms_event_running_.store(true);
        oms_event_thread_ = std::thread([this]() {
            std::cout << "[TRADER_LIB] OMS event polling thread started" << std::endl;
            int poll_count = 0;
            while (oms_event_running_.load()) {
                oms_adapter_->poll_events();
                poll_count++;
                if (poll_count % 100 == 0) {
                    std::cout << "[TRADER_LIB] OMS polling count: " << poll_count << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            std::cout << "[TRADER_LIB] OMS event polling thread stopped" << std::endl;
        });
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
    
    // Stop OMS event polling thread
    if (oms_event_running_.load()) {
        std::cout << "[TRADER_LIB] Stopping OMS event polling" << std::endl;
        oms_event_running_.store(false);
        if (oms_event_thread_.joinable()) {
            oms_event_thread_.join();
        }
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
        std::cout << "[TRADER_LIB] Stopping PMS adapter" << std::endl;
        pms_adapter_->stop();
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
    
    // Set up PMS adapter callback to forward position updates to strategy container
    if (pms_adapter_) {
        std::cout << "[TRADER_LIB] Setting up PMS adapter callback" << std::endl;
        pms_adapter_->set_position_callback([this](const proto::PositionUpdate& position) {
            std::cout << "[TRADER_LIB] PMS adapter received position update: " << position.symbol() 
                      << " qty: " << position.qty() << std::endl;
            
            // Forward position update to strategy container
            strategy_container_->on_position_update(position);
        });
    }
    
    // Set up OMS adapter callback to forward order events to strategy container
    if (oms_adapter_) {
        std::cout << "[TRADER_LIB] Setting up OMS adapter callback" << std::endl;
        oms_adapter_->set_event_callback([this](const std::string& cl_ord_id, const std::string& exch, 
                                                const std::string& symbol, uint32_t event_type, 
                                                double fill_qty, double fill_price, const std::string& text) {
            std::cout << "[TRADER_LIB] OMS adapter received order event: " << cl_ord_id 
                      << " symbol: " << symbol << " type: " << event_type << std::endl;
            
            // Convert to protobuf OrderEvent and forward to strategy container
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(cl_ord_id);
            order_event.set_exch(exch);
            order_event.set_symbol(symbol);
            order_event.set_event_type(static_cast<proto::OrderEventType>(event_type));
            order_event.set_fill_qty(fill_qty);
            order_event.set_fill_price(fill_price);
            order_event.set_text(text);
            
            strategy_container_->on_order_event(order_event);
        });
    }
}

std::shared_ptr<AbstractStrategy> TraderLib::get_strategy() const {
    if (!strategy_container_) {
        return nullptr;
    }
    
    return strategy_container_->get_strategy();
}

} // namespace trader
