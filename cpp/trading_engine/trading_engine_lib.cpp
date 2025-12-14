#include "trading_engine_lib.hpp"
#include "../utils/logging/logger.hpp"
#include "../utils/oms/order_state.hpp"
#include "../utils/constants.hpp"
#include "../utils/error_handling.hpp"
#include "../utils/metrics/metrics_collector.hpp"
#include "../utils/exchange/exchange_symbol_registry.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

namespace trading_engine {

TradingEngineLib::TradingEngineLib() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Initializing Trading Engine Library");
    
    running_.store(false);
    // exchange_name_ will be set via set_exchange() method
    
    // Initialize order state machine
    order_state_machine_ = std::make_unique<OrderStateMachine>();
    
    // Initialize rate limiting
    orders_sent_this_second_.store(0);
    last_rate_reset_ = std::chrono::steady_clock::now();
    max_orders_per_second_ = 10; // Default, can be overridden via config
    
    logger.debug("Trading Engine Library initialized");
}

TradingEngineLib::~TradingEngineLib() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Destroying Trading Engine Library");
    stop();
}

bool TradingEngineLib::initialize(const std::string& config_file) {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Initializing with config: " + config_file);
    
    try {
        // Initialize configuration manager
        config_manager_ = std::make_unique<config::ProcessConfigManager>();
        if (!config_file.empty()) {
            if (!config_manager_->load_config(config_file)) {
                logger.error("Failed to load config file: " + config_file);
                return false;
            }
        }
        
        // Setup exchange OMS
        setup_exchange_oms();
        
        // Load rate limit configuration if available
        if (config_manager_) {
            std::string rate_limit_str = config_manager_->get_string("TRADING_ENGINE", "MAX_ORDERS_PER_SECOND", "10");
            try {
                max_orders_per_second_ = std::stoi(rate_limit_str);
                logger.info("Rate limit configured: " + std::to_string(max_orders_per_second_) + " orders/second");
            } catch (const std::exception& e) {
                logger.warn("Invalid MAX_ORDERS_PER_SECOND config, using default: 10");
            }
            
            // Load exchange symbol configuration
            std::string symbol_config_path = config_manager_->get_string("TRADING_ENGINE", "EXCHANGE_INSTR_CONFIG", "exchange_instr_config.ini");
            if (!symbol_config_path.empty()) {
                auto& registry = ExchangeSymbolRegistry::get_instance();
                if (registry.load_from_config(symbol_config_path)) {
                    logger.info("Loaded exchange symbol configuration from: " + symbol_config_path);
                } else {
                    logger.warn("Failed to load exchange symbol configuration from: " + symbol_config_path);
                }
            }
        }
        
        logger.info("Initialization complete");
        return true;
        
    } catch (const std::exception& e) {
        logger.error("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void TradingEngineLib::start() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Starting Trading Engine");
    
    if (running_.load()) {
        logger.debug("Already running");
        return;
    }
    
    running_.store(true);
    
    // Start message processing thread
    message_processing_running_.store(true);
    message_processing_thread_ = std::thread(&TradingEngineLib::message_processing_loop, this);
    
    // Connect to exchange OMS
    if (exchange_oms_) {
        if (!exchange_oms_->connect()) {
            logger.error("Failed to connect to exchange OMS");
            handle_error("Failed to connect to exchange OMS");
        } else {
            logger.info("Connected to exchange OMS");
            
            // Query open orders from exchange for startup state recovery
            // This ensures we have complete order state before processing new orders
            query_open_orders_at_startup();
        }
    }
    
    logger.info("Trading Engine started");
}

void TradingEngineLib::stop() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Stopping Trading Engine");
    
    if (!running_.load()) {
        logger.debug("Already stopped");
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
        logger.debug("Disconnected from exchange OMS");
    }
    
    logger.info("Trading Engine stopped");
}

bool TradingEngineLib::send_order(const std::string& cl_ord_id, const std::string& symbol, 
                                 proto::Side side, proto::OrderType type, double qty, double price) {
    logging::Logger logger("TRADING_ENGINE");
    if (!running_.load() || !exchange_oms_) {
        logger.error("Cannot send order: not running or no exchange OMS");
        return false;
    }
    
    // Check for duplicate order ID
    {
        std::lock_guard<std::mutex> lock(order_states_mutex_);
        if (order_states_.find(cl_ord_id) != order_states_.end()) {
            logger.error("Duplicate order ID detected: " + cl_ord_id + " - rejecting order");
            return false;
        }
    }
    
    // Validate order parameters using exchange symbol registry
    // Note: Strategy should have already rounded prices/sizes, so we only validate here
    auto& registry = ExchangeSymbolRegistry::get_instance();
    if (!registry.validate_only(exchange_name_, symbol, qty, price)) {
        logger.error("Order validation failed for: " + cl_ord_id);
        return false;
    }
    
    // Validate order parameters
    if (!validate_order(symbol, qty, price, type)) {
        logger.error("Order validation failed for: " + cl_ord_id);
        return false;
    }
    
    // Check rate limit before sending
    if (!check_rate_limit()) {
        logger.warn("Rate limit exceeded - throttling order: " + cl_ord_id);
        return false;
    }
    
    std::stringstream ss;
    ss << "Sending order: " << cl_ord_id << " " << symbol 
       << " " << (side == proto::Side::BUY ? "BUY" : "SELL") 
       << " " << qty << "@" << price;
    logger.debug(ss.str());
    
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
        orders_sent_this_second_.fetch_add(1); // Increment rate limit counter
        
        // Store order state but mark as SENT (not PENDING) until exchange ACKs
        // This prevents optimistic state updates
        OrderStateInfo order_state;
        order_state.cl_ord_id = cl_ord_id;
        order_state.symbol = symbol;
        order_state.side = (side == proto::Side::BUY ? Side::Buy : Side::Sell);
        order_state.qty = qty;
        order_state.price = price;
        order_state.is_market = (type == proto::OrderType::MARKET);
        order_state.state = OrderState::PENDING; // Will be updated to ACKNOWLEDGED when exchange confirms
        order_state.exch = exchange_name_;
        order_state.created_time = std::chrono::system_clock::now();
        order_state.last_update_time = order_state.created_time;
        
        {
            std::lock_guard<std::mutex> lock(order_states_mutex_);
            order_states_[cl_ord_id] = order_state;
        }
        
        logger.debug("Order sent successfully - waiting for exchange ACK");
    } else {
        logger.error("Failed to send order");
        handle_error("Failed to send order to exchange");
        // Create REJECTED order state for failed orders
        OrderStateInfo rejected_state;
        rejected_state.cl_ord_id = cl_ord_id;
        rejected_state.symbol = symbol;
        rejected_state.state = OrderState::REJECTED;
        rejected_state.reject_reason = "Failed to send to exchange";
        rejected_state.created_time = std::chrono::system_clock::now();
        rejected_state.last_update_time = rejected_state.created_time;
        
        {
            std::lock_guard<std::mutex> lock(order_states_mutex_);
            order_states_[cl_ord_id] = rejected_state;
        }
    }
    
    return success;
}

bool TradingEngineLib::cancel_order(const std::string& cl_ord_id) {
    logging::Logger logger("TRADING_ENGINE");
    if (!running_.load() || !exchange_oms_) {
        logger.error("Cannot cancel order: not running or no exchange OMS");
        return false;
    }
    
    logger.debug("Cancelling order: " + cl_ord_id);
    
    // Send to exchange OMS
    bool success = exchange_oms_->cancel_order(cl_ord_id, "");
    
    if (success) {
        logger.debug("Cancel request sent successfully");
    } else {
        logger.error("Failed to send cancel request");
        handle_error("Failed to send cancel request to exchange");
    }
    
    return success;
}

bool TradingEngineLib::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    logging::Logger logger("TRADING_ENGINE");
    if (!running_.load() || !exchange_oms_) {
        logger.error("Cannot modify order: not running or no exchange OMS");
        return false;
    }
    
    std::stringstream ss;
    ss << "Modifying order: " << cl_ord_id << " new_price=" << new_price << " new_qty=" << new_qty;
    logger.debug(ss.str());
    
    // Create modify request
    proto::OrderRequest modify_request;
    modify_request.set_cl_ord_id(cl_ord_id);
    modify_request.set_price(new_price);
    modify_request.set_qty(new_qty);
    
    // Send to exchange OMS
    bool success = exchange_oms_->replace_order(cl_ord_id, modify_request);
    
    if (success) {
        logger.debug("Modify request sent successfully");
    } else {
        logger.error("Failed to send modify request");
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
    logging::Logger logger("TRADING_ENGINE");
    if (exchange_name_.empty()) {
        logger.error("Exchange name not set. Call set_exchange() first.");
        return;
    }
    
    logger.info("Setting up exchange OMS for: " + exchange_name_);
    
    if (!config_manager_) {
        logger.error("Configuration manager not initialized");
        return;
    }
    
    // Create exchange OMS using factory (like other libraries)
    exchange_oms_ = exchanges::OMSFactory::create(exchange_name_);
    
    if (!exchange_oms_) {
        logger.error("Failed to create exchange OMS for: " + exchange_name_);
        return;
    }
    
    // Set up callbacks
    exchange_oms_->set_order_status_callback([this](const proto::OrderEvent& order_event) {
        handle_order_event(order_event);
    });
    
    logger.debug("Exchange OMS setup complete");
}

void TradingEngineLib::query_open_orders_at_startup() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Querying open orders from exchange for startup state recovery");
    
    if (exchange_name_.empty()) {
        logger.error("Cannot query open orders: exchange name not set");
        return;
    }
    
    if (!config_manager_) {
        logger.error("Cannot query open orders: configuration manager not initialized");
        return;
    }
    
    // Get API credentials from config (using exchange-specific section)
    std::string section = exchange_name_;
    std::transform(section.begin(), section.end(), section.begin(), ::toupper);
    
    std::string api_key = config_manager_->get_string(section, "API_KEY", "");
    std::string api_secret = config_manager_->get_string(section, "API_SECRET", "");
    
    if (api_key.empty()) {
        logger.warn("Cannot query open orders: API_KEY not found in config section [" + section + "]");
        return;
    }
    
    // Create data fetcher for this exchange
    auto data_fetcher = exchanges::DataFetcherFactory::create(exchange_name_, api_key, api_secret);
    if (!data_fetcher) {
        logger.error("Failed to create data fetcher for exchange: " + exchange_name_);
        return;
    }
    
    // Query open orders from exchange
    try {
        auto open_orders = data_fetcher->get_open_orders();
        logger.info("Queried " + std::to_string(open_orders.size()) + " open orders from exchange");
        
        // Process each open order: update local state and publish via ZMQ
        for (const auto& order_event : open_orders) {
            // Create order state entry if it doesn't exist
            {
                std::lock_guard<std::mutex> lock(order_states_mutex_);
                auto it = order_states_.find(order_event.cl_ord_id());
                if (it == order_states_.end()) {
                    // Create new order state entry from order event
                    // Extract metadata from text field (contains origQty, side, price from data fetcher)
                    OrderStateInfo order_state;
                    order_state.cl_ord_id = order_event.cl_ord_id();
                    order_state.symbol = order_event.symbol();
                    order_state.exch = order_event.exch();
                    order_state.exchange_order_id = order_event.exch_order_id();
                    
                    // Parse metadata from text field (format: "origQty:<value>|side:<value>|price:<value>")
                    // Use defaults that are safe if parsing fails
                    double orig_qty = order_event.fill_qty();  // Default to fill_qty if not in metadata
                    std::string side_str = "BUY";
                    double price = order_event.fill_price();  // Default to fill_price if not in metadata
                    
                    if (!order_event.text().empty()) {
                        try {
                            std::istringstream iss(order_event.text());
                            std::string token;
                            while (std::getline(iss, token, '|')) {
                                size_t colon_pos = token.find(':');
                                if (colon_pos != std::string::npos) {
                                    std::string key = token.substr(0, colon_pos);
                                    std::string value = token.substr(colon_pos + 1);
                                    if (key == "origQty") {
                                        try {
                                            orig_qty = std::stod(value);
                                        } catch (const std::exception& e) {
                                            logger.warn("Failed to parse origQty from metadata: " + value + 
                                                       " - using default: " + std::to_string(orig_qty));
                                        }
                                    } else if (key == "side") {
                                        side_str = value;
                                    } else if (key == "price") {
                                        try {
                                            price = std::stod(value);
                                        } catch (const std::exception& e) {
                                            logger.warn("Failed to parse price from metadata: " + value + 
                                                       " - using default: " + std::to_string(price));
                                        }
                                    }
                                }
                            }
                        } catch (const std::exception& e) {
                            logger.warn("Failed to parse order metadata text field: " + order_event.text() + 
                                       " - using defaults. Error: " + e.what());
                        }
                    }
                    
                    // Set order details
                    order_state.qty = orig_qty;  // Original order quantity
                    order_state.filled_qty = order_event.fill_qty();  // Filled quantity
                    order_state.price = price;  // Limit price
                    order_state.avg_fill_price = order_event.fill_price();  // Average fill price
                    order_state.side = (side_str == "SELL" || side_str == "sell") ? Side::Sell : Side::Buy;
                    order_state.is_market = false;  // Open orders are typically limit orders
                    order_state.created_time = std::chrono::system_clock::time_point(
                        std::chrono::microseconds(order_event.timestamp_us()));
                    order_state.last_update_time = order_state.created_time;
                    
                    // Set initial state based on event type and fill status
                    // For partially filled orders: filled_qty < qty and event_type is FILL
                    bool is_partially_filled = (order_event.event_type() == proto::OrderEventType::FILL) &&
                                               (order_state.filled_qty > 0.0) &&
                                               (order_state.filled_qty < order_state.qty);
                    
                    switch (order_event.event_type()) {
                        case proto::OrderEventType::ACK:
                            order_state.state = OrderState::ACKNOWLEDGED;
                            break;
                        case proto::OrderEventType::FILL:
                            if (is_partially_filled) {
                                order_state.state = OrderState::PARTIALLY_FILLED;
                            } else {
                                // Fully filled (shouldn't be in open orders, but handle it)
                                order_state.state = OrderState::FILLED;
                            }
                            break;
                        case proto::OrderEventType::CANCEL:
                            order_state.state = OrderState::CANCELLED;
                            break;
                        case proto::OrderEventType::REJECT:
                            order_state.state = OrderState::REJECTED;
                            break;
                        default:
                            order_state.state = OrderState::PENDING;
                            break;
                    }
                    
                    order_states_[order_event.cl_ord_id()] = order_state;
                } else {
                    // Update existing order state
                    update_order_state(order_event.cl_ord_id(), order_event.event_type());
                }
            }
            
            // Publish order event via ZMQ so Trader receives it
            publish_order_event(order_event);
            
            logger.debug("Synced open order: " + order_event.cl_ord_id() + 
                        " symbol: " + order_event.symbol() + 
                        " status: " + std::to_string(static_cast<int>(order_event.event_type())));
        }
        
        if (open_orders.empty()) {
            logger.info("No open orders found on exchange");
        } else {
            logger.info("Successfully synced " + std::to_string(open_orders.size()) + 
                       " open orders to local state and published via ZMQ");
        }
    } catch (const std::exception& e) {
        logger.error("Exception while querying open orders: " + std::string(e.what()));
        handle_error("Failed to query open orders: " + std::string(e.what()));
    }
}

void TradingEngineLib::message_processing_loop() {
    logging::Logger logger("TRADING_ENGINE");
    logger.debug("Starting message processing loop");
    
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
                    logging::Logger logger("TRADING_ENGINE");
                    logger.error("Failed to parse order request message");
                    statistics_.parse_errors.fetch_add(1);
                }
            } catch (const std::exception& e) {
                logging::Logger logger("TRADING_ENGINE");
                logger.error("Error processing message: " + std::string(e.what()));
                statistics_.parse_errors.fetch_add(1);
            }
            
            lock.lock();
        }
    }
    
    logger.debug("Message processing loop stopped");
}

void TradingEngineLib::handle_order_request(const proto::OrderRequest& order_request) {
    logging::Logger logger("TRADING_ENGINE");
    
    // Use metrics timer for performance tracking
    auto timer = METRICS_TIMER("trading_engine.order_request_processing_us").start();
    
    // Check if this is a cancel request (symbol == "__CANCEL__")
    if (order_request.symbol() == "__CANCEL__") {
        logger.debug("Handling cancel request: " + order_request.cl_ord_id());
        cancel_order(order_request.cl_ord_id());
        return;
    }
    
    // Check if this is a modify request (symbol == "__MODIFY__")
    if (order_request.symbol() == "__MODIFY__") {
        logger.debug("Handling modify request: " + order_request.cl_ord_id() + 
                    " new_price=" + std::to_string(order_request.price()) + 
                    " new_qty=" + std::to_string(order_request.qty()));
        modify_order(order_request.cl_ord_id(), order_request.price(), order_request.qty());
        return;
    }
    
    // Only log at DEBUG level for normal operations
    logger.debug("Handling order request: " + order_request.cl_ord_id());
    
    statistics_.orders_received.fetch_add(1);
    METRICS_COUNTER("trading_engine.orders_received").increment();
    
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
            METRICS_COUNTER("trading_engine.orders_sent_to_exchange").increment();
        } else {
            METRICS_COUNTER("trading_engine.order_send_failures").increment();
            logger.error("Failed to send order: " + order_request.cl_ord_id());
        }
    }
}

void TradingEngineLib::handle_order_event(const proto::OrderEvent& order_event) {
    logging::Logger logger("TRADING_ENGINE");
    
    // Use metrics timer for performance tracking
    auto timer = METRICS_TIMER("trading_engine.order_event_processing_us").start();
    
    // Only log at DEBUG level for normal operations (reduces verbosity)
    logger.debug("Handling order event: " + order_event.cl_ord_id() + 
                " event_type=" + std::to_string(static_cast<int>(order_event.event_type())));
    
    // Validate order event before processing
    if (order_event.cl_ord_id().empty()) {
        logger.error("Received order event with empty cl_ord_id - ignoring");
        statistics_.parse_errors.fetch_add(1);
        METRICS_COUNTER("trading_engine.parse_errors").increment();
        return;
    }
    
    // Atomic update of order state (prevents race conditions)
    // Update filled quantity and state in single critical section
    {
        std::lock_guard<std::mutex> lock(order_states_mutex_);
        auto it = order_states_.find(order_event.cl_ord_id());
        if (it != order_states_.end()) {
            // Store exchange_order_id when available (usually in ACK event)
            if (!order_event.exch_order_id().empty()) {
                it->second.exchange_order_id = order_event.exch_order_id();
            }
            
            // Update filled quantity if FILL event
            if (order_event.event_type() == proto::OrderEventType::FILL) {
                it->second.filled_qty = order_event.fill_qty();
                if (order_event.fill_price() > 0.0 && it->second.filled_qty > 0.0) {
                    it->second.avg_fill_price = order_event.fill_price();
                }
            }
            
            // Update state atomically with filled_qty (no race window)
            // Use internal helper to avoid double locking
            update_order_state_internal(it, order_event.event_type());
        } else {
            // Order not found - still update statistics and publish event
            logger.warn("Order state not found for: " + order_event.cl_ord_id() + 
                       " - event will be published but state not updated");
        }
    }
    
    // Update statistics and metrics
    switch (order_event.event_type()) {
        case proto::OrderEventType::ACK:
            statistics_.orders_acked.fetch_add(1);
            METRICS_COUNTER("trading_engine.orders_acked").increment();
            break;
        case proto::OrderEventType::FILL:
            statistics_.orders_filled.fetch_add(1);
            METRICS_COUNTER("trading_engine.orders_filled").increment();
            if (order_event.fill_qty() > 0.0) {
                METRICS_GAUGE("trading_engine.total_filled_volume").increment(order_event.fill_qty());
            }
            break;
        case proto::OrderEventType::CANCEL:
            statistics_.orders_cancelled.fetch_add(1);
            METRICS_COUNTER("trading_engine.orders_cancelled").increment();
            break;
        case proto::OrderEventType::REJECT:
            statistics_.orders_rejected.fetch_add(1);
            METRICS_COUNTER("trading_engine.orders_rejected").increment();
            logger.warn("Order rejected: " + order_event.cl_ord_id());  // WARN level for rejections
            break;
        default:
            break;
    }
    
    // Publish order event
    publish_order_event(order_event);
    
    // Call user callback with exception handling
    error_handling::safe_callback(order_event_callback_, "TRADING_ENGINE", 
                                  "order event", order_event);
    // Track callback errors if they occurred (safe_callback logs internally)
    // Note: We could enhance safe_callback to return error status if needed
}

void TradingEngineLib::handle_error(const std::string& error_message) {
    logging::Logger logger("TRADING_ENGINE");
    logger.error("Error: " + error_message);
    
    statistics_.connection_errors.fetch_add(1);
    
    // Call user callback with exception handling
    error_handling::safe_callback(error_callback_, "TRADING_ENGINE", 
                                  "error", error_message);
}

void TradingEngineLib::publish_order_event(const proto::OrderEvent& order_event) {
    logging::Logger logger("TRADING_ENGINE");
    if (publisher_) {
        std::string message;
        if (order_event.SerializeToString(&message)) {
            std::string topic = "order_events";
            logger.debug("Publishing order event to ZMQ topic: " + topic + 
                        " cl_ord_id: " + order_event.cl_ord_id() + 
                        " symbol: " + order_event.symbol() + 
                        " size: " + std::to_string(message.size()) + " bytes");
            bool success = publisher_->publish(topic, message);
            if (success) {
                statistics_.zmq_messages_sent.fetch_add(1);
            } else {
                statistics_.zmq_messages_dropped.fetch_add(1);
                // Warning already logged by ZmqPublisher
            }
        } else {
            logger.error("Failed to serialize order event");
        }
    } else {
        logger.error("No publisher available for order event");
    }
}

void TradingEngineLib::update_order_state(const std::string& cl_ord_id, proto::OrderEventType event_type) {
    logging::Logger logger("TRADING_ENGINE");
    std::lock_guard<std::mutex> lock(order_states_mutex_);
    
    auto it = order_states_.find(cl_ord_id);
    if (it == order_states_.end()) {
        logger.warn("Order state not found for: " + cl_ord_id + " - cannot update state");
        return;
    }
    
    update_order_state_internal(it, event_type);
}

// Internal helper that assumes lock is already held (prevents double locking)
void TradingEngineLib::update_order_state_internal(std::map<std::string, OrderStateInfo>::iterator it, 
                                                   proto::OrderEventType event_type) {
    logging::Logger logger("TRADING_ENGINE");
    
    OrderState current_state = it->second.state;
    OrderState new_state = current_state;
    
    // Map proto event type to OrderState with validation
    switch (event_type) {
        case proto::OrderEventType::ACK:
            if (current_state != OrderState::PENDING) {
                logger.warn("Invalid state transition: ACK from " + 
                           std::string(to_string(current_state)) + " for order " + it->second.cl_ord_id);
            }
            new_state = OrderState::ACKNOWLEDGED;
            break;
        case proto::OrderEventType::FILL:
            // Check if this is a partial or full fill
            // filled_qty should already be updated by caller
            if (it->second.filled_qty >= it->second.qty - constants::order::FILLED_QTY_EPSILON) {  // Account for floating point precision
                new_state = OrderState::FILLED;
            } else if (current_state == OrderState::ACKNOWLEDGED || 
                      current_state == OrderState::PARTIALLY_FILLED) {
                new_state = OrderState::PARTIALLY_FILLED;
            } else {
                // Unexpected state, but transition to filled anyway
                logger.warn("FILL event from unexpected state: " + 
                           std::string(to_string(current_state)) + " for order " + it->second.cl_ord_id);
                new_state = OrderState::FILLED;
            }
            break;
        case proto::OrderEventType::CANCEL:
            if (current_state == OrderState::FILLED) {
                logger.warn("Cannot cancel already filled order: " + it->second.cl_ord_id);
                return;  // Don't update state
            }
            new_state = OrderState::CANCELLED;
            break;
        case proto::OrderEventType::REJECT:
            new_state = OrderState::REJECTED;
            break;
        default:
            logger.warn("Unknown event type: " + std::to_string(static_cast<int>(event_type)) + 
                       " for order " + it->second.cl_ord_id);
            return;  // Don't update state for unknown events
    }
    
    // Validate state transition
    if (new_state != current_state && 
        !OrderStateMachine::isValidTransition(current_state, new_state)) {
        logger.error("Invalid state transition: " + std::string(to_string(current_state)) + 
                    " -> " + std::string(to_string(new_state)) + " for order " + it->second.cl_ord_id);
        // Still update to prevent stuck states, but log the error
    }
    
    it->second.state = new_state;
    it->second.last_update_time = std::chrono::system_clock::now();
}

void TradingEngineLib::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    if (exchange_oms_) {
        exchange_oms_->set_websocket_transport(transport);
    }
}

bool TradingEngineLib::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    update_rate_limit(); // Reset counter if needed
    return orders_sent_this_second_.load() < max_orders_per_second_;
}

void TradingEngineLib::update_rate_limit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_rate_reset_).count();
    
    if (elapsed >= 1) {
        orders_sent_this_second_.store(0);
        last_rate_reset_ = now;
    }
}

bool TradingEngineLib::validate_order(const std::string& symbol, double qty, double price, proto::OrderType type) {
    logging::Logger logger("TRADING_ENGINE");
    
    // Basic validation
    if (qty <= 0.0) {
        logger.error("Invalid order quantity: " + std::to_string(qty));
        return false;
    }
    
    if (type == proto::OrderType::LIMIT && price <= 0.0) {
        logger.error("Invalid price for limit order: " + std::to_string(price));
        return false;
    }
    
    // Exchange-specific validation using symbol registry
    auto& registry = ExchangeSymbolRegistry::get_instance();
    ExchangeSymbolInfo symbol_info = registry.get_symbol_info(exchange_name_, symbol);
    
    if (symbol_info.is_valid) {
        // Validate against exchange-specific constraints
        if (!registry.validate_order_params(symbol_info, qty, price)) {
            logger.error("Order validation failed for " + exchange_name_ + ":" + symbol +
                        " qty=" + std::to_string(qty) + " price=" + std::to_string(price));
            return false;
        }
    } else {
        // No symbol info available - log warning but allow order
        logger.debug("No symbol info for " + exchange_name_ + ":" + symbol + " - skipping exchange-specific validation");
    }
    
    return true;
}

void TradingEngineLib::on_reconnected() {
    logging::Logger logger("TRADING_ENGINE");
    logger.info("Exchange reconnected - reconciling order state");
    
    // Re-query open orders to sync state with exchange
    query_open_orders_at_startup();
}

} // namespace trading_engine