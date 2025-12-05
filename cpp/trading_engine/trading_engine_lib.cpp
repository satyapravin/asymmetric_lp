#include "trading_engine_lib.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace trading_engine {

TradingEngineLib::TradingEngineLib() {
    std::cout << "[TRADING_ENGINE] Initializing Trading Engine Library" << std::endl;
    
    running_.store(false);
    // exchange_name_ will be set via set_exchange() method
    
    // Initialize order state machine
    order_state_machine_ = std::make_unique<OrderStateMachine>();
    
    std::cout << "[TRADING_ENGINE] Trading Engine Library initialized" << std::endl;
}

TradingEngineLib::~TradingEngineLib() {
    std::cout << "[TRADING_ENGINE] Destroying Trading Engine Library" << std::endl;
    stop();
}

bool TradingEngineLib::initialize(const std::string& config_file) {
    std::cout << "[TRADING_ENGINE] Initializing with config: " << config_file << std::endl;
    
    try {
        // Initialize configuration manager
        config_manager_ = std::make_unique<config::ProcessConfigManager>();
        if (!config_file.empty()) {
            if (!config_manager_->load_config(config_file)) {
                std::cerr << "[TRADING_ENGINE] Failed to load config file: " << config_file << std::endl;
                return false;
            }
        }
        
        // Setup exchange OMS
        setup_exchange_oms();
        
        std::cout << "[TRADING_ENGINE] Initialization complete" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngineLib::start() {
    std::cout << "[TRADING_ENGINE] Starting Trading Engine" << std::endl;
    
    if (running_.load()) {
        std::cout << "[TRADING_ENGINE] Already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    // Start message processing thread
    message_processing_running_.store(true);
    message_processing_thread_ = std::thread(&TradingEngineLib::message_processing_loop, this);
    
    // Connect to exchange OMS
    if (exchange_oms_) {
        if (!exchange_oms_->connect()) {
            std::cerr << "[TRADING_ENGINE] Failed to connect to exchange OMS" << std::endl;
            handle_error("Failed to connect to exchange OMS");
        } else {
            std::cout << "[TRADING_ENGINE] Connected to exchange OMS" << std::endl;
        }
    }
    
    std::cout << "[TRADING_ENGINE] Trading Engine started" << std::endl;
}

void TradingEngineLib::stop() {
    std::cout << "[TRADING_ENGINE] Stopping Trading Engine" << std::endl;
    
    if (!running_.load()) {
        std::cout << "[TRADING_ENGINE] Already stopped" << std::endl;
        return;
    }
    
    running_.store(false);
    
    // Stop message processing thread
    message_processing_running_.store(false);
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        message_cv_.notify_all();
    }
    
    if (message_processing_thread_.joinable()) {
        message_processing_thread_.join();
    }
    
    // Disconnect from exchange OMS
    if (exchange_oms_) {
        exchange_oms_->disconnect();
        std::cout << "[TRADING_ENGINE] Disconnected from exchange OMS" << std::endl;
    }
    
    std::cout << "[TRADING_ENGINE] Trading Engine stopped" << std::endl;
}

bool TradingEngineLib::send_order(const std::string& cl_ord_id, const std::string& symbol, 
                                 proto::Side side, proto::OrderType type, double qty, double price) {
    if (!running_.load() || !exchange_oms_) {
        std::cerr << "[TRADING_ENGINE] Cannot send order: not running or no exchange OMS" << std::endl;
        return false;
    }
    
    std::cout << "[TRADING_ENGINE] Sending order: " << cl_ord_id << " " << symbol 
              << " " << (side == proto::Side::BUY ? "BUY" : "SELL") 
              << " " << qty << "@" << price << std::endl;
    
    // Create order request
    proto::OrderRequest order_request;
    order_request.set_cl_ord_id(cl_ord_id);
    order_request.set_symbol(symbol);
    order_request.set_side(side);
    order_request.set_type(type);
    order_request.set_qty(qty);
    order_request.set_price(price);
    order_request.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // Send to exchange OMS
    bool success = false;
    if (type == proto::OrderType::MARKET) {
        success = exchange_oms_->place_market_order(symbol, 
            (side == proto::Side::BUY ? "BUY" : "SELL"), qty);
    } else if (type == proto::OrderType::LIMIT) {
        success = exchange_oms_->place_limit_order(symbol, 
            (side == proto::Side::BUY ? "BUY" : "SELL"), qty, price);
    }
    
    if (success) {
        statistics_.orders_sent_to_exchange.fetch_add(1);
        
        // Update order state
        OrderStateInfo order_state;
        order_state.cl_ord_id = cl_ord_id;
        order_state.symbol = symbol;
        order_state.side = (side == proto::Side::BUY ? Side::Buy : Side::Sell);
        order_state.qty = qty;
        order_state.price = price;
        order_state.is_market = (type == proto::OrderType::MARKET);
        order_state.state = OrderState::PENDING;
        order_state.created_time = std::chrono::system_clock::now();
        order_state.last_update_time = order_state.created_time;
        
        {
            std::lock_guard<std::mutex> lock(order_states_mutex_);
            order_states_[cl_ord_id] = order_state;
        }
        
        std::cout << "[TRADING_ENGINE] Order sent successfully" << std::endl;
    } else {
        std::cerr << "[TRADING_ENGINE] Failed to send order" << std::endl;
        handle_error("Failed to send order to exchange");
    }
    
    return success;
}

bool TradingEngineLib::cancel_order(const std::string& cl_ord_id) {
    if (!running_.load() || !exchange_oms_) {
        std::cerr << "[TRADING_ENGINE] Cannot cancel order: not running or no exchange OMS" << std::endl;
        return false;
    }
    
    std::cout << "[TRADING_ENGINE] Cancelling order: " << cl_ord_id << std::endl;
    
    // Send to exchange OMS
    bool success = exchange_oms_->cancel_order(cl_ord_id, "");
    
    if (success) {
        std::cout << "[TRADING_ENGINE] Cancel request sent successfully" << std::endl;
    } else {
        std::cerr << "[TRADING_ENGINE] Failed to send cancel request" << std::endl;
        handle_error("Failed to send cancel request to exchange");
    }
    
    return success;
}

bool TradingEngineLib::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    if (!running_.load() || !exchange_oms_) {
        std::cerr << "[TRADING_ENGINE] Cannot modify order: not running or no exchange OMS" << std::endl;
        return false;
    }
    
    std::cout << "[TRADING_ENGINE] Modifying order: " << cl_ord_id 
              << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
    
    // Create modify request
    proto::OrderRequest modify_request;
    modify_request.set_cl_ord_id(cl_ord_id);
    modify_request.set_price(new_price);
    modify_request.set_qty(new_qty);
    
    // Send to exchange OMS
    bool success = exchange_oms_->replace_order(cl_ord_id, modify_request);
    
    if (success) {
        std::cout << "[TRADING_ENGINE] Modify request sent successfully" << std::endl;
    } else {
        std::cerr << "[TRADING_ENGINE] Failed to send modify request" << std::endl;
        handle_error("Failed to send modify request to exchange");
    }
    
    return success;
}

std::optional<OrderStateInfo> TradingEngineLib::get_order_state(const std::string& cl_ord_id) const {
    std::lock_guard<std::mutex> lock(order_states_mutex_);
    
    auto it = order_states_.find(cl_ord_id);
    if (it != order_states_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<OrderStateInfo> TradingEngineLib::get_active_orders() const {
    std::lock_guard<std::mutex> lock(order_states_mutex_);
    
    std::vector<OrderStateInfo> active_orders;
    
    for (const auto& [cl_ord_id, order_state] : order_states_) {
        if (order_state.state == OrderState::ACKNOWLEDGED || 
            order_state.state == OrderState::PARTIALLY_FILLED) {
            active_orders.push_back(order_state);
        }
    }
    
    return active_orders;
}

std::vector<OrderStateInfo> TradingEngineLib::get_all_orders() const {
    std::lock_guard<std::mutex> lock(order_states_mutex_);
    
    std::vector<OrderStateInfo> all_orders;
    all_orders.reserve(order_states_.size());
    
    for (const auto& [cl_ord_id, order_state] : order_states_) {
        all_orders.push_back(order_state);
    }
    
    return all_orders;
}

void TradingEngineLib::setup_exchange_oms() {
    if (exchange_name_.empty()) {
        std::cerr << "[TRADING_ENGINE] Exchange name not set. Call set_exchange() first." << std::endl;
        return;
    }
    
    std::cout << "[TRADING_ENGINE] Setting up exchange OMS for: " << exchange_name_ << std::endl;
    
    if (!config_manager_) {
        std::cerr << "[TRADING_ENGINE] Configuration manager not initialized" << std::endl;
        return;
    }
    
    // Create exchange OMS using factory (like other libraries)
    exchange_oms_ = exchanges::OMSFactory::create(exchange_name_);
    
    if (!exchange_oms_) {
        std::cerr << "[TRADING_ENGINE] Failed to create exchange OMS for: " << exchange_name_ << std::endl;
        return;
    }
    
    // Set up callbacks
    exchange_oms_->set_order_status_callback([this](const proto::OrderEvent& order_event) {
        handle_order_event(order_event);
    });
    
    std::cout << "[TRADING_ENGINE] Exchange OMS setup complete" << std::endl;
}

void TradingEngineLib::message_processing_loop() {
    std::cout << "[TRADING_ENGINE] Starting message processing loop" << std::endl;
    
    while (message_processing_running_.load()) {
        std::unique_lock<std::mutex> lock(message_queue_mutex_);
        
        if (message_queue_.empty()) {
            message_cv_.wait(lock, [this] { 
                return !message_queue_.empty() || !message_processing_running_.load(); 
            });
        }
        
        if (!message_processing_running_.load()) {
            break;
        }
        
        while (!message_queue_.empty()) {
            std::string message = message_queue_.front();
            message_queue_.pop();
            lock.unlock();
            
            // Process message
            try {
                proto::OrderRequest order_request;
                if (order_request.ParseFromString(message)) {
                    handle_order_request(order_request);
                    statistics_.zmq_messages_received.fetch_add(1);
                } else {
                    std::cerr << "[TRADING_ENGINE] Failed to parse order request message" << std::endl;
                    statistics_.parse_errors.fetch_add(1);
                }
            } catch (const std::exception& e) {
                std::cerr << "[TRADING_ENGINE] Error processing message: " << e.what() << std::endl;
                statistics_.parse_errors.fetch_add(1);
            }
            
            lock.lock();
        }
    }
    
    std::cout << "[TRADING_ENGINE] Message processing loop stopped" << std::endl;
}

void TradingEngineLib::handle_order_request(const proto::OrderRequest& order_request) {
    std::cout << "[TRADING_ENGINE] Handling order request: " << order_request.cl_ord_id() << std::endl;
    
    statistics_.orders_received.fetch_add(1);
    
    // Send order to exchange
    if (exchange_oms_) {
        bool success = false;
        if (order_request.type() == proto::OrderType::MARKET) {
            success = exchange_oms_->place_market_order(order_request.symbol(), 
                (order_request.side() == proto::Side::BUY ? "BUY" : "SELL"), order_request.qty());
        } else if (order_request.type() == proto::OrderType::LIMIT) {
            success = exchange_oms_->place_limit_order(order_request.symbol(), 
                (order_request.side() == proto::Side::BUY ? "BUY" : "SELL"), order_request.qty(), order_request.price());
        }
        if (success) {
            statistics_.orders_sent_to_exchange.fetch_add(1);
        }
    }
}

void TradingEngineLib::handle_order_event(const proto::OrderEvent& order_event) {
    std::cout << "[TRADING_ENGINE] Handling order event: " << order_event.cl_ord_id() 
              << " event_type=" << order_event.event_type() << std::endl;
    
    // Update order state
    update_order_state(order_event.cl_ord_id(), order_event.event_type());
    
    // Update statistics
    switch (order_event.event_type()) {
        case proto::OrderEventType::ACK:
            statistics_.orders_acked.fetch_add(1);
            break;
        case proto::OrderEventType::FILL:
            statistics_.orders_filled.fetch_add(1);
            break;
        case proto::OrderEventType::CANCEL:
            statistics_.orders_cancelled.fetch_add(1);
            break;
        case proto::OrderEventType::REJECT:
            statistics_.orders_rejected.fetch_add(1);
            break;
        default:
            break;
    }
    
    // Publish order event
    publish_order_event(order_event);
    
    // Call user callback
    if (order_event_callback_) {
        order_event_callback_(order_event);
    }
}

void TradingEngineLib::handle_error(const std::string& error_message) {
    std::cerr << "[TRADING_ENGINE] Error: " << error_message << std::endl;
    
    statistics_.connection_errors.fetch_add(1);
    
    // Call user callback
    if (error_callback_) {
        error_callback_(error_message);
    }
}

void TradingEngineLib::publish_order_event(const proto::OrderEvent& order_event) {
    if (publisher_) {
        std::string message;
        if (order_event.SerializeToString(&message)) {
            std::string topic = "order_events";
            std::cout << "[TRADING_ENGINE] Publishing order event to ZMQ topic: " << topic 
                      << " cl_ord_id: " << order_event.cl_ord_id() 
                      << " symbol: " << order_event.symbol() 
                      << " size: " << message.size() << " bytes" << std::endl;
            publisher_->publish(topic, message);
            statistics_.zmq_messages_sent.fetch_add(1);
            std::cout << "[TRADING_ENGINE] Order event published successfully" << std::endl;
        } else {
            std::cerr << "[TRADING_ENGINE] Failed to serialize order event" << std::endl;
        }
    } else {
        std::cerr << "[TRADING_ENGINE] No publisher available for order event" << std::endl;
    }
}

void TradingEngineLib::update_order_state(const std::string& cl_ord_id, proto::OrderEventType event_type) {
    std::lock_guard<std::mutex> lock(order_states_mutex_);
    
    auto it = order_states_.find(cl_ord_id);
    if (it != order_states_.end()) {
        // Map proto event type to OrderState
        switch (event_type) {
            case proto::OrderEventType::ACK:
                it->second.state = OrderState::ACKNOWLEDGED;
                break;
            case proto::OrderEventType::FILL:
                it->second.state = OrderState::FILLED;
                break;
            case proto::OrderEventType::CANCEL:
                it->second.state = OrderState::CANCELLED;
                break;
            case proto::OrderEventType::REJECT:
                it->second.state = OrderState::REJECTED;
                break;
            default:
                break;
        }
        it->second.last_update_time = std::chrono::system_clock::now();
    }
}

void TradingEngineLib::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    if (exchange_oms_) {
        exchange_oms_->set_websocket_transport(transport);
    }
}

} // namespace trading_engine