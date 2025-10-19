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
#include "../utils/config/process_config_manager.hpp"

// Include concrete OMS implementations for factory
#include "../exchanges/binance/private_websocket/binance_oms.hpp"
#include "../exchanges/grvt/private_websocket/grvt_oms.hpp"
#include "../exchanges/deribit/private_websocket/deribit_oms.hpp"

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
        // Initialize exchange OMS using factory pattern
        oms_ = OMSFactory::create_oms(config_.exchange_name, config_);
        if (!oms_) {
            std::cerr << "[TRADING_ENGINE] Failed to create OMS for exchange: " << config_.exchange_name << std::endl;
            return false;
        }
        
        // Connect to exchange
        if (!oms_->connect()) {
            std::cerr << "[TRADING_ENGINE] Failed to connect to " << config_.exchange_name << std::endl;
            return false;
        }
        
        // Set up exchange callbacks
        oms_->set_order_status_callback([this](const proto::OrderEvent& order_event) {
            handle_order_update(order_event.cl_ord_id(), order_event.event_type() == proto::OrderEventType::FILL ? "FILLED" : "ACKNOWLEDGED");
        });
        
        // Initialize exchange subscriber (if available)
        subscriber_ = OMSFactory::create_subscriber(config_.exchange_name, config_);
        if (subscriber_) {
            std::cout << "[TRADING_ENGINE] Subscriber created for " << config_.exchange_name << std::endl;
        }
        
        // Initialize ZMQ publishers
        order_events_publisher_ = std::make_unique<ZmqPublisher>(config_.order_events_pub_endpoint);
        trade_events_publisher_ = std::make_unique<ZmqPublisher>(config_.trade_events_pub_endpoint);
        order_status_publisher_ = std::make_unique<ZmqPublisher>(config_.order_status_pub_endpoint);
        
        // Initialize ZMQ subscribers
        trader_subscriber_ = std::make_unique<ZmqSubscriber>(config_.trader_sub_endpoint, "trader_orders");
        position_server_subscriber_ = std::make_unique<ZmqSubscriber>(config_.position_server_sub_endpoint, "position_updates");
        
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
    
    // Disconnect from exchange
    if (oms_) {
        oms_->disconnect();
        oms_.reset();
    }
    
    if (subscriber_) {
        subscriber_.reset();
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
        bool order_sent = false;
        std::string exchange_order_id;
        
        // Send order to exchange using interface
        if (oms_) {
            std::string side = request.side == "BUY" ? "BUY" : "SELL";
            std::string order_type = request.order_type == "MARKET" ? "MARKET" : "LIMIT";
            
            if (order_type == "MARKET") {
                order_sent = oms_->place_market_order(request.symbol, side, request.qty);
            } else {
                order_sent = oms_->place_limit_order(request.symbol, side, request.qty, request.price);
            }
            
            if (order_sent) {
                exchange_order_id = request.cl_ord_id; // Use client order ID
            }
        } else {
            std::cerr << "[TRADING_ENGINE] No valid exchange OMS available" << std::endl;
        }
        
        if (order_sent) {
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
            OrderResponse response = convert_to_order_response(request.request_id, request.cl_ord_id,
                                                            "", "REJECTED", "Order failed");
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

void TradingEngine::cancel_order(const std::string& cl_ord_id) {
    std::cout << "[TRADING_ENGINE] Canceling order: " << cl_ord_id << std::endl;
    
    if (oms_) {
        // IExchangeOMS::cancel_order expects (cl_ord_id, exch_ord_id)
        // For now, we'll use cl_ord_id for both
        bool cancelled = oms_->cancel_order(cl_ord_id, cl_ord_id);
        if (cancelled) {
            std::cout << "[TRADING_ENGINE] Order cancelled successfully: " << cl_ord_id << std::endl;
            handle_order_update(cl_ord_id, "CANCELLED");
        } else {
            std::cerr << "[TRADING_ENGINE] Failed to cancel order: " << cl_ord_id << std::endl;
        }
    } else {
        std::cerr << "[TRADING_ENGINE] No OMS available to cancel order" << std::endl;
    }
}

void TradingEngine::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    std::cout << "[TRADING_ENGINE] Modifying order: " << cl_ord_id 
              << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
    
    if (oms_) {
        // IExchangeOMS::replace_order expects (cl_ord_id, OrderRequest)
        // Create a new OrderRequest with the modified values
        proto::OrderRequest new_order;
        new_order.set_cl_ord_id(cl_ord_id);
        new_order.set_price(new_price);
        new_order.set_qty(new_qty);
        
        bool modified = oms_->replace_order(cl_ord_id, new_order);
        if (modified) {
            std::cout << "[TRADING_ENGINE] Order modified successfully: " << cl_ord_id << std::endl;
            handle_order_update(cl_ord_id, "MODIFIED");
        } else {
            std::cerr << "[TRADING_ENGINE] Failed to modify order: " << cl_ord_id << std::endl;
        }
    } else {
        std::cerr << "[TRADING_ENGINE] No OMS available to modify order" << std::endl;
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
            auto message = trader_subscriber_->receive();
            if (message.has_value()) {
                handle_trader_message(message.value());
            }
            
            // Check for messages from position server
            auto pos_message = position_server_subscriber_->receive();
            if (pos_message.has_value()) {
                handle_position_server_message(pos_message.value());
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
        order.side = Side::Buy;
    } else if (request.side == "SELL") {
        order.side = Side::Sell;
    }
    
    // Convert order type
    if (request.order_type == "MARKET") {
        order.is_market = true;
    } else if (request.order_type == "LIMIT") {
        order.is_market = false;
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
        
        order_events_publisher_->publish("order_events", message);
        
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
        
        trade_events_publisher_->publish("trade_events", message);
        
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
        
        order_status_publisher_->publish("order_status", message);
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADING_ENGINE] Error publishing order status: " << e.what() << std::endl;
    }
}

bool TradingEngine::is_healthy() const {
    bool exchange_connected = oms_ && oms_->is_connected();
    
    return initialized_ && running_ && exchange_connected;
}

std::map<std::string, std::string> TradingEngine::get_health_status() const {
    std::map<std::string, std::string> status;
    status["initialized"] = initialized_ ? "true" : "false";
    status["running"] = running_ ? "true" : "false";
    
    bool exchange_connected = oms_ && oms_->is_connected();
    
    status["exchange_connected"] = exchange_connected ? "true" : "false";
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

// OMSFactory implementation
std::unique_ptr<IExchangeOMS> OMSFactory::create_oms(const std::string& exchange_name, const TradingEngineConfig& config) {
    if (exchange_name == "BINANCE") {
        binance::BinanceConfig oms_config;
        oms_config.api_key = config.api_key;
        oms_config.api_secret = config.api_secret;
        oms_config.base_url = config.http_base_url;
        oms_config.testnet = config.testnet_mode;
        
        return std::make_unique<binance::BinanceOMS>(oms_config);
    }
    else if (exchange_name == "GRVT") {
        grvt::GrvtOMSConfig oms_config;
        oms_config.api_key = config.api_key;
        oms_config.session_cookie = config.api_secret;
        oms_config.account_id = config.api_key;
        oms_config.base_url = config.http_base_url;
        oms_config.websocket_url = config.ws_private_url;
        oms_config.testnet = config.testnet_mode;
        
        return std::make_unique<grvt::GrvtOMS>(oms_config);
    }
    else if (exchange_name == "DERIBIT") {
        deribit::DeribitOMSConfig oms_config;
        oms_config.client_id = config.api_key;
        oms_config.client_secret = config.api_secret;
        oms_config.base_url = config.http_base_url;
        oms_config.websocket_url = config.ws_private_url;
        oms_config.testnet = config.testnet_mode;
        
        return std::make_unique<deribit::DeribitOMS>(oms_config);
    }
    
    throw std::runtime_error("Unsupported exchange: " + exchange_name);
}

std::unique_ptr<IExchangeSubscriber> OMSFactory::create_subscriber(const std::string& exchange_name, const TradingEngineConfig& config) {
    // Subscriber functionality will be implemented when IExchangeSubscriber interface is available
    return nullptr;
}

// TradingEngineFactory implementation
std::unique_ptr<TradingEngine> TradingEngineFactory::create_trading_engine(const std::string& exchange_name, const std::string& config_file) {
    TradingEngineConfig config = load_config(exchange_name, config_file);
    return std::make_unique<TradingEngine>(config);
}

TradingEngineConfig TradingEngineFactory::load_config(const std::string& exchange_name, const std::string& config_file) {
    TradingEngineConfig config;
    
    // Load from provided config file
    config::ProcessConfigManager config_manager;
    config_manager.load_config(config_file);
    
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
    
    // Exchange-specific settings (handled by exchange interfaces)
    config.http_base_url = config_manager.get_string("HTTP_API", "HTTP_BASE_URL", "");
    config.ws_private_url = config_manager.get_string("WEBSOCKET", "WS_PRIVATE_URL", "");
    config.enable_private_websocket = config_manager.get_bool("WEBSOCKET", "ENABLE_PRIVATE_WEBSOCKET", true);
    config.enable_http_api = config_manager.get_bool("HTTP_API", "ENABLE_HTTP_API", true);
    
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
