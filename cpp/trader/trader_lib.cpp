#include "trader_lib.hpp"
#include "../utils/config/process_config_manager.hpp"
#include "../utils/logging/logger.hpp"
#include "../utils/constants.hpp"
#include <mutex>

namespace trader {

TraderLib::TraderLib() : running_(false), oms_event_running_(false) {
    logging::Logger logger("TRADER_LIB");
    logger.info("Initializing Trader Library");
}

TraderLib::~TraderLib() {
    stop();
    logging::Logger logger("TRADER_LIB");
    logger.info("Destroying Trader Library");
}

bool TraderLib::initialize(const std::string& config_file) {
    logging::Logger logger("TRADER_LIB");
    logger.info("Initializing with config: " + config_file);
    
    // Initialize configuration manager
    config_manager_ = std::make_unique<config::ProcessConfigManager>();
    if (!config_file.empty()) {
        if (!config_manager_->load_config(config_file)) {
            logger.error("Failed to load config file: " + config_file);
            return false;
        }
    }
    
    // Create strategy container
    strategy_container_ = std::make_unique<StrategyContainer>();
    
    // Create ZMQ adapters based on configuration
    // Load endpoints from config with sensible defaults
    std::string mds_endpoint = config_manager_ ? 
        config_manager_->get_string("SUBSCRIBERS", "MARKET_SERVER_SUB_ENDPOINT", "tcp://127.0.0.1:5555") :
        "tcp://127.0.0.1:5555";
    std::string pms_endpoint = config_manager_ ?
        config_manager_->get_string("SUBSCRIBERS", "POSITION_SERVER_SUB_ENDPOINT", "tcp://127.0.0.1:5556") :
        "tcp://127.0.0.1:5556";
    std::string oms_publish_endpoint = config_manager_ ?
        config_manager_->get_string("PUBLISHERS", "ORDER_EVENTS_PUB_ENDPOINT", "tcp://127.0.0.1:5557") :
        "tcp://127.0.0.1:5557";
    std::string oms_subscribe_endpoint = config_manager_ ?
        config_manager_->get_string("SUBSCRIBERS", "TRADING_ENGINE_SUB_ENDPOINT", "tcp://127.0.0.1:5558") :
        "tcp://127.0.0.1:5558";
    // Create MDS adapter
    mds_adapter_ = std::make_shared<ZmqMDSAdapter>(mds_endpoint, "market_data", exchange_);
    logger.debug("Created MDS adapter for endpoint: " + mds_endpoint);
    
    // Create PMS adapter
    pms_adapter_ = std::make_shared<ZmqPMSAdapter>(pms_endpoint, "position_updates");
    logger.debug("Created PMS adapter for endpoint: " + pms_endpoint);
    
    // Create OMS adapter
    oms_adapter_ = std::make_shared<ZmqOMSAdapter>(oms_publish_endpoint, "orders", oms_subscribe_endpoint, "order_events");
    logger.debug("Created OMS adapter for endpoints: " + oms_publish_endpoint + " / " + oms_subscribe_endpoint);
    
    return true;
}

void TraderLib::start() {
    logging::Logger logger("TRADER_LIB");
    logger.info("Starting Trader Library");
    
    if (running_.load()) {
        logger.debug("Already running");
        return;
    }
    
    // Start strategy container
    if (strategy_container_) {
        strategy_container_->start();
    }
    
    // Start OMS adapter polling
    if (oms_adapter_) {
        logger.debug("Starting OMS adapter polling");
        oms_event_running_.store(true);
        oms_event_thread_ = std::thread([this]() {
            logging::Logger thread_logger("TRADER_LIB");
            thread_logger.debug("OMS event polling thread started");
            int poll_count = 0;
            while (oms_event_running_.load()) {
                oms_adapter_->poll_events();
                poll_count++;
                if (poll_count % constants::polling::OMS_LOG_INTERVAL == 0) {
                    thread_logger.debug("OMS polling count: " + std::to_string(poll_count));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(constants::polling::OMS_EVENT_POLL_INTERVAL_MS));
            }
            thread_logger.debug("OMS event polling thread stopped");
        });
    }
    
    running_.store(true);
    logger.info("Started successfully");
}

void TraderLib::stop() {
    logging::Logger logger("TRADER_LIB");
    logger.info("Stopping Trader Library");
    
    if (!running_.load()) {
        logger.debug("Already stopped");
        return;
    }
    
    // Stop strategy container
    if (strategy_container_) {
        strategy_container_->stop();
    }
    
    // Stop OMS event polling thread
    if (oms_event_running_.load()) {
        logger.debug("Stopping OMS event polling");
        oms_event_running_.store(false);
        if (oms_event_thread_.joinable()) {
            oms_event_thread_.join();
        }
    }
    
    // Stop ZMQ adapters
    if (mds_adapter_) {
        logger.debug("Stopping MDS adapter");
        mds_adapter_->stop();
    }
    if (oms_adapter_) {
        // TODO: Add stop() method to OMS adapter
    }
    if (pms_adapter_) {
        logger.debug("Stopping PMS adapter");
        pms_adapter_->stop();
    }
    
    running_.store(false);
    logger.info("Stopped successfully");
}

void TraderLib::set_strategy(std::shared_ptr<AbstractStrategy> strategy) {
    logging::Logger logger("TRADER_LIB");
    logger.info("Setting strategy");
    
    if (!strategy_container_) {
        logger.error("Strategy container not initialized!");
        return;
    }
    
    strategy_container_->set_strategy(strategy);
    
    // Set up MDS adapter callback to forward market data to strategy container
    if (mds_adapter_) {
        logger.debug("Setting up MDS adapter callback");
        mds_adapter_->on_snapshot = [this](const proto::OrderBookSnapshot& orderbook) {
            logging::Logger callback_logger("TRADER_LIB");
            callback_logger.debug("MDS adapter received orderbook: " + orderbook.symbol() + 
                                 " bids: " + std::to_string(orderbook.bids_size()) + 
                                 " asks: " + std::to_string(orderbook.asks_size()));
            
            // Forward proto::OrderBookSnapshot directly to strategy container
            strategy_container_->on_market_data(orderbook);
        };
    }
    
    // Set up PMS adapter callbacks to forward position and balance updates to strategy container
    if (pms_adapter_) {
        logger.debug("Setting up PMS adapter callbacks");
        
        // Position update callback
        pms_adapter_->set_position_callback([this](const proto::PositionUpdate& position) {
            logging::Logger callback_logger("TRADER_LIB");
            callback_logger.debug("PMS adapter received position update: " + position.symbol() + 
                                 " qty: " + std::to_string(position.qty()));
            
            // Forward position update to strategy container
            strategy_container_->on_position_update(position);
        });
        
        // Balance update callback
        pms_adapter_->set_balance_callback([this](const proto::AccountBalanceUpdate& balance) {
            logging::Logger callback_logger("TRADER_LIB");
            callback_logger.debug("PMS adapter received balance update: " + std::to_string(balance.balances_size()) + " balances");
            
            // Forward balance update to strategy container
            strategy_container_->on_account_balance_update(balance);
        });
    }
    
    // Set up OMS adapter callback to forward order events to strategy container
    if (oms_adapter_) {
        logger.debug("Setting up OMS adapter callback");
        oms_adapter_->set_event_callback([this](const std::string& cl_ord_id, const std::string& exch, 
                                                const std::string& symbol, uint32_t event_type, 
                                                double fill_qty, double fill_price, const std::string& text) {
            logging::Logger callback_logger("TRADER_LIB");
            callback_logger.debug("OMS adapter received order event: " + cl_ord_id + 
                                 " symbol: " + symbol + " type: " + std::to_string(event_type));
            
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

void TraderLib::handle_order_event(const proto::OrderEvent& order_event) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (order_event_callback_) {
            order_event_callback_(order_event);
        }
    }
    
    // Update statistics
    statistics_.zmq_messages_received.fetch_add(1);
}

void TraderLib::handle_market_data(const proto::OrderBookSnapshot& orderbook) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (market_data_callback_) {
            market_data_callback_(orderbook);
        }
    }
    
    // Update statistics
    statistics_.market_data_received.fetch_add(1);
}

void TraderLib::handle_position_update(const proto::PositionUpdate& position) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (position_update_callback_) {
            position_update_callback_(position);
        }
    }
    
    // Update statistics
    statistics_.position_updates.fetch_add(1);
}

void TraderLib::handle_balance_update(const proto::AccountBalanceUpdate& balance) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (balance_update_callback_) {
            balance_update_callback_(balance);
        }
    }
    
    // Update statistics
    statistics_.balance_updates.fetch_add(1);
}

void TraderLib::handle_trade_execution(const proto::Trade& trade) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (trade_execution_callback_) {
            trade_execution_callback_(trade);
        }
    }
    
    // Update statistics
    statistics_.trade_executions.fetch_add(1);
}

void TraderLib::handle_error(const std::string& error_message) {
    // Invoke callback with mutex protection
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (error_callback_) {
            error_callback_(error_message);
        }
    }
    
    // Update statistics
    statistics_.strategy_errors.fetch_add(1);
    
    // Log error
    logging::Logger logger("TRADER_LIB");
    logger.error("Error: " + error_message);
}

// Testing interface - simulate events for testing
void TraderLib::simulate_market_data(const proto::OrderBookSnapshot& orderbook) {
    if (strategy_container_) {
        strategy_container_->on_market_data(orderbook);
    }
    handle_market_data(orderbook);
}

void TraderLib::simulate_order_event(const proto::OrderEvent& order_event) {
    if (strategy_container_) {
        strategy_container_->on_order_event(order_event);
    }
    handle_order_event(order_event);
}

void TraderLib::simulate_position_update(const proto::PositionUpdate& position) {
    if (strategy_container_) {
        strategy_container_->on_position_update(position);
    }
    handle_position_update(position);
}

void TraderLib::simulate_balance_update(const proto::AccountBalanceUpdate& balance) {
    if (strategy_container_) {
        strategy_container_->on_account_balance_update(balance);
    }
    handle_balance_update(balance);
}

void TraderLib::simulate_trade_execution(const proto::Trade& trade) {
    if (strategy_container_) {
        strategy_container_->on_trade_execution(trade);
    }
    handle_trade_execution(trade);
}

} // namespace trader
