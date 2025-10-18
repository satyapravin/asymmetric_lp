#include "trading_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <json/json.h>

namespace trading_engine {

TradingEngine::TradingEngine(const TradingEngineConfig& config) 
    : config_(config) {
    std::cout << "[TRADING_ENGINE] Initializing trading engine for " << config.exchange_name << std::endl;
}

TradingEngine::~TradingEngine() {
    shutdown();
}

bool TradingEngine::initialize() {
    std::cout << "[TRADING_ENGINE] Initializing trading engine..." << std::endl;
    
    try {
        // Initialize HTTP handler for private API calls
        if (config_.enable_http_api) {
            if (!initialize_http_handler()) {
                std::cerr << "[TRADING_ENGINE] Failed to initialize HTTP handler" << std::endl;
                return false;
            }
            http_connected_ = true;
            std::cout << "[TRADING_ENGINE] HTTP handler initialized successfully" << std::endl;
        }
        
        // Initialize WebSocket manager for private channels
        if (config_.enable_private_websocket) {
            if (!initialize_websocket_manager()) {
                std::cerr << "[TRADING_ENGINE] Failed to initialize WebSocket manager" << std::endl;
                return false;
            }
            
            if (!connect_private_websocket()) {
                std::cerr << "[TRADING_ENGINE] Failed to connect private WebSocket" << std::endl;
                return false;
            }
            
            if (!subscribe_to_private_channels()) {
                std::cerr << "[TRADING_ENGINE] Failed to subscribe to private channels" << std::endl;
                return false;
            }
            
            websocket_connected_ = true;
            std::cout << "[TRADING_ENGINE] Private WebSocket connected successfully" << std::endl;
        }
        
        // Initialize exchange OMS (for backward compatibility)
        binance::BinanceConfig oms_config;
        oms_config.api_key = config_.api_key;
        oms_config.api_secret = config_.api_secret;
        oms_config.asset_type = config_.asset_type;
        oms_config.exchange_name = config_.exchange_name;
        
        oms_ = std::make_unique<binance::BinanceOMS>(oms_config);
        
        // Connect to exchange
        auto connect_result = oms_->connect();
        if (!connect_result.is_success()) {
            std::cerr << "[TRADING_ENGINE] Failed to connect to exchange: " 
                      << connect_result.get_error().error_message << std::endl;
            return false;
        }
        
        // Initialize ZMQ publishers
        order_events_publisher_ = std::make_unique<ZmqPublisher>(config_.order_events_pub_endpoint);
        trade_events_publisher_ = std::make_unique<ZmqPublisher>(config_.trade_events_pub_endpoint);
        order_status_publisher_ = std::make_unique<ZmqPublisher>(config_.order_status_pub_endpoint);
        
        // Initialize ZMQ subscribers
        trader_subscriber_ = std::make_unique<ZmqSubscriber>(config_.trader_sub_endpoint);
        position_server_subscriber_ = std::make_unique<ZmqSubscriber>(config_.position_server_sub_endpoint);
        
        // Set up exchange callbacks
        oms_->set_order_event_callback([this](const std::string& order_id, const std::string& status) {
            handle_order_update(order_id, status);
        });
        
        oms_->set_trade_event_callback([this](const std::string& trade_id, double qty, double price) {
            handle_trade_update(trade_id, qty, price);
        });
        
        // Initialize rate limiting
        last_rate_reset_ = std::chrono::steady_clock::now();
        
        initialized_ = true;
        std::cout << "[TRADING_ENGINE] Initialization completed successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngine::run() {
    if (!initialized_) {
        std::cerr << "[TRADING_ENGINE] Not initialized, cannot run" << std::endl;
        return;
    }
    
    std::cout << "[TRADING_ENGINE] Starting trading engine..." << std::endl;
    
    running_ = true;
    
    // Start processing threads
    order_processing_thread_ = std::thread(&TradingEngine::order_processing_loop, this);
    zmq_subscriber_thread_ = std::thread(&TradingEngine::zmq_subscriber_loop, this);
    
    // Start WebSocket message processing thread if WebSocket is enabled
    if (config_.enable_private_websocket) {
        websocket_message_thread_ = std::thread(&TradingEngine::websocket_message_loop, this);
    }
    
    // Main loop
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Update rate limiting
        update_rate_limit();
        
        // Health check
        if (!is_healthy()) {
            std::cerr << "[TRADING_ENGINE] Health check failed" << std::endl;
            break;
        }
    }
    
    std::cout << "[TRADING_ENGINE] Trading engine stopped" << std::endl;
}

void TradingEngine::shutdown() {
    std::cout << "[TRADING_ENGINE] Shutting down trading engine..." << std::endl;
    
    running_ = false;
    
    // Notify order processing thread
    {
        std::lock_guard<std::mutex> lock(order_mutex_);
        order_cv_.notify_all();
    }
    
    // Wait for threads to finish
    if (order_processing_thread_.joinable()) {
        order_processing_thread_.join();
    }
    
    if (zmq_subscriber_thread_.joinable()) {
        zmq_subscriber_thread_.join();
    }
    
    if (websocket_message_thread_.joinable()) {
        websocket_message_thread_.join();
    }
    
    // Disconnect WebSocket if connected
    if (config_.enable_private_websocket && ws_manager_) {
        disconnect_private_websocket();
    }
    
    // Disconnect from exchange
    if (oms_) {
        oms_->disconnect();
    }
    
    std::cout << "[TRADING_ENGINE] Shutdown completed" << std::endl;
}

void TradingEngine::process_order_request(const OrderRequest& request) {
    std::cout << "[TRADING_ENGINE] Processing order request: " << request.cl_ord_id << std::endl;
    
    // Check rate limit
    if (!check_rate_limit()) {
        std::cerr << "[TRADING_ENGINE] Rate limit exceeded, rejecting order: " << request.cl_ord_id << std::endl;
        
        OrderResponse response = convert_to_order_response(request.request_id, request.cl_ord_id, 
                                                        "", "REJECTED", "Rate limit exceeded");
        publish_order_event(response);
        return;
    }
    
    // Add to processing queue
    {
        std::lock_guard<std::mutex> lock(order_mutex_);
        pending_orders_[request.cl_ord_id] = request;
        order_queue_.push(request);
        order_cv_.notify_one();
    }
}

void TradingEngine::order_processing_loop() {
    std::cout << "[TRADING_ENGINE] Order processing thread started" << std::endl;
    
    while (running_) {
        std::unique_lock<std::mutex> lock(order_mutex_);
        order_cv_.wait(lock, [this] { return !order_queue_.empty() || !running_; });
        
        if (!running_) {
            break;
        }
        
        if (!order_queue_.empty()) {
            OrderRequest request = order_queue_.front();
            order_queue_.pop();
            lock.unlock();
            
            process_order_queue_item(request);
        }
    }
    
    std::cout << "[TRADING_ENGINE] Order processing thread stopped" << std::endl;
}

void TradingEngine::process_order_queue_item(const OrderRequest& request) {
    try {
        // Convert to exchange order
        Order exchange_order = convert_to_exchange_order(request);
        
        // Send order to exchange
        auto result = oms_->send_order(exchange_order);
        
        if (result.is_success()) {
            std::string exchange_order_id = result.get_value();
            
            // Store response
            OrderResponse response = convert_to_order_response(request.request_id, request.cl_ord_id,
                                                            exchange_order_id, "ACKNOWLEDGED");
            
            {
                std::lock_guard<std::mutex> lock(order_mutex_);
                order_responses_[request.cl_ord_id] = response;
            }
            
            // Publish order event
            publish_order_event(response);
            
            // Update metrics
            total_orders_sent_++;
            orders_sent_this_second_++;
            
            std::cout << "[TRADING_ENGINE] Order sent successfully: " << request.cl_ord_id 
                      << " -> " << exchange_order_id << std::endl;
            
        } else {
            // Order failed
            std::cerr << "[TRADING_ENGINE] Order failed: " << request.cl_ord_id 
                      << " - " << result.get_error().error_message << std::endl;
            
            OrderResponse response = convert_to_order_response(request.request_id, request.cl_ord_id,
                                                            "", "REJECTED", result.get_error().error_message);
            publish_order_event(response);
            
            total_orders_rejected_++;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception processing order: " << request.cl_ord_id 
                  << " - " << e.what() << std::endl;
        
        OrderResponse response = convert_to_order_response(request.request_id, request.cl_ord_id,
                                                        "", "REJECTED", e.what());
        publish_order_event(response);
        
        total_orders_rejected_++;
    }
}

void TradingEngine::handle_order_update(const std::string& order_id, const std::string& status) {
    std::cout << "[TRADING_ENGINE] Order update: " << order_id << " status: " << status << std::endl;
    
    // Find the corresponding client order ID
    std::string cl_ord_id;
    {
        std::lock_guard<std::mutex> lock(order_mutex_);
        for (const auto& [client_id, request] : pending_orders_) {
            if (request.cl_ord_id == order_id) {
                cl_ord_id = client_id;
                break;
            }
        }
    }
    
    if (!cl_ord_id.empty()) {
        // Update order response
        {
            std::lock_guard<std::mutex> lock(order_mutex_);
            if (order_responses_.find(cl_ord_id) != order_responses_.end()) {
                order_responses_[cl_ord_id].status = status;
                order_responses_[cl_ord_id].timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            }
        }
        
        // Publish order status update
        publish_order_status(cl_ord_id, status);
        
        // Update metrics
        if (status == "FILLED") {
            total_orders_filled_++;
        } else if (status == "CANCELLED") {
            total_orders_cancelled_++;
        }
        
        // Remove from pending orders if terminal state
        if (status == "FILLED" || status == "CANCELLED" || status == "REJECTED") {
            std::lock_guard<std::mutex> lock(order_mutex_);
            pending_orders_.erase(cl_ord_id);
        }
    }
}

void TradingEngine::handle_trade_update(const std::string& trade_id, double qty, double price) {
    std::cout << "[TRADING_ENGINE] Trade execution: " << trade_id << " " << qty << "@" << price << std::endl;
    
    // Create trade execution report
    TradeExecution execution;
    execution.trade_id = trade_id;
    execution.qty = qty;
    execution.price = price;
    execution.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Publish trade event
    publish_trade_event(execution);
    
    // Update metrics
    total_trades_executed_++;
}

void TradingEngine::zmq_subscriber_loop() {
    std::cout << "[TRADING_ENGINE] ZMQ subscriber thread started" << std::endl;
    
    while (running_) {
        try {
            // Check for messages from trader
            if (trader_subscriber_->has_message()) {
                std::string message = trader_subscriber_->receive_message();
                handle_trader_message(message);
            }
            
            // Check for messages from position server
            if (position_server_subscriber_->has_message()) {
                std::string message = position_server_subscriber_->receive_message();
                handle_position_server_message(message);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            std::cerr << "[TRADING_ENGINE] ZMQ subscriber error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "[TRADING_ENGINE] ZMQ subscriber thread stopped" << std::endl;
}

void TradingEngine::handle_trader_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(message, root)) {
            std::cerr << "[TRADING_ENGINE] Failed to parse trader message" << std::endl;
            return;
        }
        
        std::string message_type = root.get("type", "").asString();
        
        if (message_type == "ORDER_REQUEST") {
            OrderRequest request;
            request.request_id = root["request_id"].asString();
            request.cl_ord_id = root["cl_ord_id"].asString();
            request.symbol = root["symbol"].asString();
            request.side = root["side"].asString();
            request.qty = root["qty"].asDouble();
            request.price = root["price"].asDouble();
            request.order_type = root.get("order_type", "LIMIT").asString();
            request.time_in_force = root.get("time_in_force", "GTC").asString();
            request.timestamp_us = root.get("timestamp_us", 0).asUInt64();
            
            process_order_request(request);
            
        } else if (message_type == "CANCEL_ORDER") {
            std::string cl_ord_id = root["cl_ord_id"].asString();
            cancel_order(cl_ord_id);
            
        } else if (message_type == "MODIFY_ORDER") {
            std::string cl_ord_id = root["cl_ord_id"].asString();
            double new_price = root["new_price"].asDouble();
            double new_qty = root["new_qty"].asDouble();
            modify_order(cl_ord_id, new_price, new_qty);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error handling trader message: " << e.what() << std::endl;
    }
}

void TradingEngine::handle_position_server_message(const std::string& message) {
    // Handle position server messages (e.g., position updates, risk limits)
    std::cout << "[TRADING_ENGINE] Received position server message" << std::endl;
}

bool TradingEngine::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    return orders_sent_this_second_ < config_.max_orders_per_second;
}

void TradingEngine::update_rate_limit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_rate_reset_).count();
    
    if (elapsed >= 1) {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        orders_sent_this_second_ = 0;
        last_rate_reset_ = now;
    }
}

Order TradingEngine::convert_to_exchange_order(const OrderRequest& request) {
    Order order;
    order.cl_ord_id = request.cl_ord_id;
    order.symbol = request.symbol;
    order.qty = request.qty;
    order.price = request.price;
    
    // Convert side
    if (request.side == "BUY") {
        order.side = OrderSide::BUY;
    } else if (request.side == "SELL") {
        order.side = OrderSide::SELL;
    }
    
    // Convert order type
    if (request.order_type == "LIMIT") {
        order.type = OrderType::LIMIT;
    } else if (request.order_type == "MARKET") {
        order.type = OrderType::MARKET;
    }
    
    // Convert time in force
    if (request.time_in_force == "GTC") {
        order.time_in_force = TimeInForce::GTC;
    } else if (request.time_in_force == "IOC") {
        order.time_in_force = TimeInForce::IOC;
    } else if (request.time_in_force == "FOK") {
        order.time_in_force = TimeInForce::FOK;
    }
    
    return order;
}

OrderResponse TradingEngine::convert_to_order_response(const std::string& request_id, 
                                                     const std::string& cl_ord_id,
                                                     const std::string& exchange_order_id,
                                                     const std::string& status,
                                                     const std::string& error_message) {
    OrderResponse response;
    response.request_id = request_id;
    response.cl_ord_id = cl_ord_id;
    response.exchange_order_id = exchange_order_id;
    response.status = status;
    response.error_message = error_message;
    response.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

void TradingEngine::publish_order_event(const OrderResponse& response) {
    try {
        Json::Value root;
        root["type"] = "ORDER_RESPONSE";
        root["request_id"] = response.request_id;
        root["cl_ord_id"] = response.cl_ord_id;
        root["exchange_order_id"] = response.exchange_order_id;
        root["status"] = response.status;
        root["error_message"] = response.error_message;
        root["timestamp_us"] = static_cast<Json::UInt64>(response.timestamp_us);
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, root);
        
        order_events_publisher_->publish(message);
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing order event: " << e.what() << std::endl;
    }
}

void TradingEngine::publish_trade_event(const TradeExecution& execution) {
    try {
        Json::Value root;
        root["type"] = "TRADE_EXECUTION";
        root["trade_id"] = execution.trade_id;
        root["cl_ord_id"] = execution.cl_ord_id;
        root["exchange_order_id"] = execution.exchange_order_id;
        root["symbol"] = execution.symbol;
        root["side"] = execution.side;
        root["qty"] = execution.qty;
        root["price"] = execution.price;
        root["commission"] = execution.commission;
        root["timestamp_us"] = static_cast<Json::UInt64>(execution.timestamp_us);
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, root);
        
        trade_events_publisher_->publish(message);
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing trade event: " << e.what() << std::endl;
    }
}

void TradingEngine::publish_order_status(const std::string& cl_ord_id, const std::string& status) {
    try {
        Json::Value root;
        root["type"] = "ORDER_STATUS_UPDATE";
        root["cl_ord_id"] = cl_ord_id;
        root["status"] = status;
        root["timestamp_us"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, root);
        
        order_status_publisher_->publish(message);
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing order status: " << e.what() << std::endl;
    }
}

bool TradingEngine::is_healthy() const {
    return initialized_ && running_ && oms_ && oms_->is_connected();
}

std::map<std::string, std::string> TradingEngine::get_health_status() const {
    std::map<std::string, std::string> status;
    status["initialized"] = initialized_ ? "true" : "false";
    status["running"] = running_ ? "true" : "false";
    status["exchange_connected"] = oms_ && oms_->is_connected() ? "true" : "false";
    status["pending_orders"] = std::to_string(pending_orders_.size());
    return status;
}

std::map<std::string, double> TradingEngine::get_performance_metrics() const {
    std::map<std::string, double> metrics;
    metrics["total_orders_sent"] = total_orders_sent_.load();
    metrics["total_orders_filled"] = total_orders_filled_.load();
    metrics["total_orders_cancelled"] = total_orders_cancelled_.load();
    metrics["total_orders_rejected"] = total_orders_rejected_.load();
    metrics["total_trades_executed"] = total_trades_executed_.load();
    metrics["orders_per_second"] = orders_sent_this_second_.load();
    return metrics;
}

// HTTP API methods
bool TradingEngine::initialize_http_handler() {
    try {
        // Create HTTP handler using factory
        http_handler_ = HttpHandlerFactory::create("CURL");
        if (!http_handler_) {
            std::cerr << "[TRADING_ENGINE] Failed to create HTTP handler" << std::endl;
            return false;
        }
        
        // Initialize HTTP handler
        if (!http_handler_->initialize()) {
            std::cerr << "[TRADING_ENGINE] Failed to initialize HTTP handler" << std::endl;
            return false;
        }
        
        // Set default timeout
        http_handler_->set_default_timeout(config_.http_timeout_ms);
        
        std::cout << "[TRADING_ENGINE] HTTP handler initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception initializing HTTP handler: " << e.what() << std::endl;
        return false;
    }
}

HttpResponse TradingEngine::make_http_request(const std::string& endpoint, const std::string& method, 
                                            const std::string& body, bool requires_signature) {
    if (!http_handler_) {
        HttpResponse response;
        response.error_message = "HTTP handler not initialized";
        response.success = false;
        return response;
    }
    
    HttpRequest request;
    request.method = method;
    request.url = config_.http_base_url + endpoint;
    request.body = body;
    request.timeout_ms = config_.http_timeout_ms;
    
    // Add default headers
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.headers["User-Agent"] = "TradingEngine/1.0";
    
    // Add authentication headers if required
    if (requires_signature) {
        auto auth_headers = create_auth_headers(method, endpoint, body);
        request.headers.insert(auth_headers.begin(), auth_headers.end());
    }
    
    return http_handler_->make_request(request);
}

std::string TradingEngine::create_auth_headers(const std::string& method, const std::string& endpoint, 
                                              const std::string& body) {
    // Create timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    // Create query string for signature
    std::string query_string = "timestamp=" + std::to_string(timestamp);
    if (!body.empty()) {
        query_string += "&" + body;
    }
    
    // Generate signature
    std::string signature = generate_signature(query_string);
    
    // Create headers
    std::map<std::string, std::string> headers;
    headers["X-MBX-APIKEY"] = config_.api_key;
    headers["timestamp"] = std::to_string(timestamp);
    headers["signature"] = signature;
    
    return query_string;
}

std::string TradingEngine::generate_signature(const std::string& data) {
    // Simple HMAC-SHA256 implementation (in production, use proper crypto library)
    // This is a placeholder - implement proper HMAC-SHA256
    return "placeholder_signature";
}

std::string TradingEngine::create_order_payload(const OrderRequest& request) {
    std::ostringstream payload;
    payload << "symbol=" << request.symbol
            << "&side=" << request.side
            << "&type=" << request.order_type
            << "&quantity=" << request.qty
            << "&price=" << request.price
            << "&timeInForce=" << request.time_in_force
            << "&newClientOrderId=" << request.cl_ord_id;
    
    return payload.str();
}

// WebSocket methods
bool TradingEngine::initialize_websocket_manager() {
    try {
        ws_manager_ = std::make_unique<binance::BinanceWebSocketManager>();
        ws_manager_->initialize(config_.api_key, config_.api_secret);
        
        // Set up callbacks
        setup_websocket_callbacks();
        
        std::cout << "[TRADING_ENGINE] WebSocket manager initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception initializing WebSocket manager: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngine::setup_websocket_callbacks() {
    if (!ws_manager_) return;
    
    // Set up order event callback
    ws_manager_->set_order_callback([this](const std::string& order_id, const std::string& status) {
        handle_order_update(order_id, status);
    });
    
    // Set up trade event callback
    ws_manager_->set_trade_callback([this](const std::string& trade_id, double qty, double price) {
        handle_trade_update(trade_id, qty, price);
    });
}

bool TradingEngine::connect_private_websocket() {
    if (!ws_manager_) {
        std::cerr << "[TRADING_ENGINE] WebSocket manager not initialized" << std::endl;
        return false;
    }
    
    try {
        if (!ws_manager_->connect_all()) {
            std::cerr << "[TRADING_ENGINE] Failed to connect WebSocket streams" << std::endl;
            return false;
        }
        
        std::cout << "[TRADING_ENGINE] Private WebSocket connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception connecting WebSocket: " << e.what() << std::endl;
        return false;
    }
}

void TradingEngine::disconnect_private_websocket() {
    if (ws_manager_) {
        ws_manager_->disconnect_all();
        std::cout << "[TRADING_ENGINE] Private WebSocket disconnected" << std::endl;
    }
}

bool TradingEngine::subscribe_to_private_channels() {
    if (!ws_manager_) {
        std::cerr << "[TRADING_ENGINE] WebSocket manager not initialized" << std::endl;
        return false;
    }
    
    try {
        // Subscribe to user data stream
        if (!ws_manager_->subscribe_to_user_data()) {
            std::cerr << "[TRADING_ENGINE] Failed to subscribe to user data stream" << std::endl;
            return false;
        }
        
        std::cout << "[TRADING_ENGINE] Subscribed to private channels successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception subscribing to private channels: " << e.what() << std::endl;
        return false;
    }
}

bool TradingEngine::is_private_websocket_connected() const {
    return ws_manager_ && ws_manager_->is_connected();
}

void TradingEngine::websocket_message_loop() {
    std::cout << "[TRADING_ENGINE] WebSocket message processing thread started" << std::endl;
    
    while (running_) {
        try {
            // Process WebSocket messages
            // The actual message processing is handled by the WebSocket manager callbacks
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            std::cerr << "[TRADING_ENGINE] WebSocket message processing error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "[TRADING_ENGINE] WebSocket message processing thread stopped" << std::endl;
}

// HTTP API operations
bool TradingEngine::send_order_via_http(const OrderRequest& request) {
    try {
        std::string payload = create_order_payload(request);
        HttpResponse response = make_http_request("/fapi/v1/order", "POST", payload, true);
        
        if (!response.success) {
            std::cerr << "[TRADING_ENGINE] HTTP order failed: " << response.error_message << std::endl;
            return false;
        }
        
        // Parse response and handle order
        // Implementation depends on exchange response format
        std::cout << "[TRADING_ENGINE] Order sent via HTTP successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception sending order via HTTP: " << e.what() << std::endl;
        return false;
    }
}

bool TradingEngine::cancel_order_via_http(const std::string& cl_ord_id) {
    try {
        std::string payload = "origClientOrderId=" + cl_ord_id;
        HttpResponse response = make_http_request("/fapi/v1/order", "DELETE", payload, true);
        
        if (!response.success) {
            std::cerr << "[TRADING_ENGINE] HTTP cancel failed: " << response.error_message << std::endl;
            return false;
        }
        
        std::cout << "[TRADING_ENGINE] Order cancelled via HTTP successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception cancelling order via HTTP: " << e.what() << std::endl;
        return false;
    }
}

bool TradingEngine::modify_order_via_http(const std::string& cl_ord_id, double new_price, double new_qty) {
    try {
        std::ostringstream payload;
        payload << "origClientOrderId=" << cl_ord_id
                << "&price=" << new_price
                << "&quantity=" << new_qty;
        
        HttpResponse response = make_http_request("/fapi/v1/order", "PUT", payload.str(), true);
        
        if (!response.success) {
            std::cerr << "[TRADING_ENGINE] HTTP modify failed: " << response.error_message << std::endl;
            return false;
        }
        
        std::cout << "[TRADING_ENGINE] Order modified via HTTP successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception modifying order via HTTP: " << e.what() << std::endl;
        return false;
    }
}

bool TradingEngine::query_order_via_http(const std::string& cl_ord_id) {
    try {
        std::string payload = "origClientOrderId=" + cl_ord_id;
        HttpResponse response = make_http_request("/fapi/v1/order", "GET", payload, true);
        
        if (!response.success) {
            std::cerr << "[TRADING_ENGINE] HTTP query failed: " << response.error_message << std::endl;
            return false;
        }
        
        std::cout << "[TRADING_ENGINE] Order queried via HTTP successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception querying order via HTTP: " << e.what() << std::endl;
        return false;
    }
}

bool TradingEngine::query_account_via_http() {
    try {
        HttpResponse response = make_http_request("/fapi/v2/account", "GET", "", true);
        
        if (!response.success) {
            std::cerr << "[TRADING_ENGINE] HTTP account query failed: " << response.error_message << std::endl;
            return false;
        }
        
        // Publish account update
        publish_account_update(response.body);
        
        std::cout << "[TRADING_ENGINE] Account queried via HTTP successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Exception querying account via HTTP: " << e.what() << std::endl;
        return false;
    }
}

// Additional event handlers
void TradingEngine::handle_account_update(const std::string& account_data) {
    std::cout << "[TRADING_ENGINE] Account update received" << std::endl;
    publish_account_update(account_data);
}

void TradingEngine::handle_balance_update(const std::string& balance_data) {
    std::cout << "[TRADING_ENGINE] Balance update received" << std::endl;
    publish_balance_update(balance_data);
}

void TradingEngine::publish_account_update(const std::string& account_data) {
    try {
        Json::Value root;
        root["type"] = "ACCOUNT_UPDATE";
        root["exchange"] = config_.exchange_name;
        root["data"] = account_data;
        root["timestamp_us"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, root);
        
        // Publish to appropriate ZMQ endpoint
        // This would go to position server or other interested parties
        std::cout << "[TRADING_ENGINE] Published account update" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing account update: " << e.what() << std::endl;
    }
}

void TradingEngine::publish_balance_update(const std::string& balance_data) {
    try {
        Json::Value root;
        root["type"] = "BALANCE_UPDATE";
        root["exchange"] = config_.exchange_name;
        root["data"] = balance_data;
        root["timestamp_us"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, root);
        
        // Publish to appropriate ZMQ endpoint
        std::cout << "[TRADING_ENGINE] Published balance update" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing balance update: " << e.what() << std::endl;
    }
}

// TradingEngineFactory implementation
std::unique_ptr<TradingEngine> TradingEngineFactory::create_trading_engine(const std::string& exchange_name) {
    TradingEngineConfig config = load_config(exchange_name);
    return std::make_unique<TradingEngine>(config);
}

TradingEngineConfig TradingEngineFactory::load_config(const std::string& exchange_name) {
    TradingEngineConfig config;
    
    // Load from config.ini
    ConfigManager config_manager;
    config_manager.load_config("config/config.ini");
    
    std::string section = "TRADING_ENGINE_" + exchange_name;
    
    config.exchange_name = exchange_name;
    config.process_name = config_manager.get_string(section, "PROCESS_NAME", "trading_engine_" + exchange_name);
    config.pid_file = config_manager.get_string(section, "PID_FILE", "/tmp/trading_engine_" + exchange_name + ".pid");
    config.log_file = config_manager.get_string(section, "LOG_FILE", "/var/log/trading/trading_engine_" + exchange_name + ".log");
    
    // Asset type
    std::string asset_type_str = config_manager.get_string(section, "ASSET_TYPE", "futures");
    if (asset_type_str == "futures") {
        config.asset_type = exchange_config::AssetType::FUTURES;
    } else if (asset_type_str == "spot") {
        config.asset_type = exchange_config::AssetType::SPOT;
    } else if (asset_type_str == "options") {
        config.asset_type = exchange_config::AssetType::OPTIONS;
    } else if (asset_type_str == "perpetual") {
        config.asset_type = exchange_config::AssetType::PERPETUAL;
    }
    
    config.api_key = config_manager.get_string(section, "API_KEY", "");
    config.api_secret = config_manager.get_string(section, "API_SECRET", "");
    config.testnet_mode = config_manager.get_bool(section, "TESTNET_MODE", false);
    
    // ZMQ endpoints
    config.order_events_pub_endpoint = config_manager.get_string(section, "ORDER_EVENTS_PUB_ENDPOINT", "");
    config.trade_events_pub_endpoint = config_manager.get_string(section, "TRADE_EVENTS_PUB_ENDPOINT", "");
    config.order_status_pub_endpoint = config_manager.get_string(section, "ORDER_STATUS_PUB_ENDPOINT", "");
    config.trader_sub_endpoint = config_manager.get_string(section, "TRADER_SUB_ENDPOINT", "");
    config.position_server_sub_endpoint = config_manager.get_string(section, "POSITION_SERVER_SUB_ENDPOINT", "");
    
    // Order management
    config.max_orders_per_second = config_manager.get_int(section, "MAX_ORDERS_PER_SECOND", 10);
    config.order_timeout_ms = config_manager.get_int(section, "ORDER_TIMEOUT_MS", 5000);
    config.retry_failed_orders = config_manager.get_bool(section, "RETRY_FAILED_ORDERS", true);
    config.max_order_retries = config_manager.get_int(section, "MAX_ORDER_RETRIES", 3);
    
    // HTTP API settings
    config.http_base_url = config_manager.get_string("HTTP_API", "HTTP_BASE_URL", "");
    config.http_timeout_ms = config_manager.get_int("HTTP_API", "HTTP_TIMEOUT_MS", 5000);
    config.http_max_retries = config_manager.get_int("HTTP_API", "HTTP_MAX_RETRIES", 3);
    config.http_retry_delay_ms = config_manager.get_int("HTTP_API", "HTTP_RETRY_DELAY_MS", 1000);
    config.enable_http_api = config_manager.get_bool("HTTP_API", "ENABLE_HTTP_API", true);
    
    // WebSocket settings
    config.ws_private_url = config_manager.get_string("WEBSOCKET", "WS_PRIVATE_URL", "");
    config.ws_private_backup_url = config_manager.get_string("WEBSOCKET", "WS_PRIVATE_BACKUP_URL", "");
    config.ws_reconnect_interval = config_manager.get_int("WEBSOCKET", "WS_RECONNECT_INTERVAL", 5000);
    config.ws_ping_interval = config_manager.get_int("WEBSOCKET", "WS_PING_INTERVAL", 30000);
    config.ws_pong_timeout = config_manager.get_int("WEBSOCKET", "WS_PONG_TIMEOUT_MS", 10000);
    config.ws_max_reconnect_attempts = config_manager.get_int("WEBSOCKET", "WS_MAX_RECONNECT_ATTEMPTS", 10);
    config.ws_connection_timeout = config_manager.get_int("WEBSOCKET", "WS_CONNECTION_TIMEOUT_MS", 10000);
    config.enable_private_websocket = config_manager.get_bool("WEBSOCKET", "ENABLE_PRIVATE_WEBSOCKET", true);
    
    // Private channels
    std::string channels_str = config_manager.get_string("WEBSOCKET", "PRIVATE_CHANNELS", "order_update,account_update");
    std::istringstream channels_stream(channels_str);
    std::string channel;
    while (std::getline(channels_stream, channel, ',')) {
        config.private_channels.push_back(channel);
    }
    
    return config;
}

std::vector<std::string> TradingEngineFactory::get_available_exchanges() {
    return {"BINANCE", "DERIBIT", "GRVT"};
}

} // namespace trading_engine
