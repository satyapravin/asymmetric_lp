#include "binance_oms.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <ctime>
#include <cstdio>

namespace binance {

BinanceOMS::BinanceOMS(const BinanceConfig& config) 
    : config_(config), current_asset_type_(config.asset_type) {
    std::cout << "[BINANCE] Initializing Binance OMS" << std::endl;
    
    // Initialize API endpoint manager
    exchange_config::initialize_api_endpoint_manager();
    
    // Load asset configuration
    asset_config_ = exchange_config::g_api_endpoint_manager->get_asset_config("BINANCE", current_asset_type_);
    if (asset_config_.base_url.empty()) {
        std::cerr << "[BINANCE] Failed to load asset configuration for type: " 
                  << exchange_config::ApiEndpointManager::asset_type_to_string(current_asset_type_) << std::endl;
    }
    
    // Initialize default handlers
    http_handler_ = HttpHandlerFactory::create("CURL");
    
    // Initialize WebSocket manager
    ws_manager_ = std::make_shared<BinanceWebSocketManager>();
    ws_manager_->initialize(config.api_key, config.api_secret);
    
    // Set up WebSocket callbacks
    ws_manager_->set_order_callback([this](const std::string& order_id, const std::string& status) {
        handle_order_update(order_id, status);
    });
    
    ws_manager_->set_trade_callback([this](const std::string& trade_id, double qty, double price) {
        handle_trade_update(trade_id, qty, price);
    });
    
    // Initialize data fetcher
    data_fetcher_ = std::make_unique<BinanceDataFetcher>();
    data_fetcher_->set_api_credentials(config.api_key, config.api_secret);
    data_fetcher_->set_base_url(asset_config_.base_url);
}

BinanceOMS::~BinanceOMS() {
    disconnect();
}

Result<bool> BinanceOMS::connect() {
    if (connected_.load()) {
        return Result<bool>(true);
    }
    
    std::cout << "[BINANCE] Connecting to Binance..." << std::endl;
    
    try {
        // Initialize HTTP handler
        if (!http_handler_->initialize()) {
            std::cout << "[BINANCE] Failed to initialize HTTP handler" << std::endl;
            return Result<bool>(ExchangeError("INIT_ERROR", "Failed to initialize HTTP handler", "BINANCE", "connect"));
        }
        
        // Initialize WebSocket manager
        if (!ws_manager_->initialize(config_.api_key, config_.api_secret)) {
            std::cout << "[BINANCE] Failed to initialize WebSocket manager" << std::endl;
            return Result<bool>(ExchangeError("INIT_ERROR", "Failed to initialize WebSocket manager", "BINANCE", "connect"));
        }
        
        // Connect WebSocket streams
        if (!ws_manager_->connect_all()) {
            std::cout << "[BINANCE] Failed to connect WebSocket streams" << std::endl;
            return Result<bool>(ExchangeError("CONNECTION_ERROR", "Failed to connect WebSocket streams", "BINANCE", "connect"));
        }
        
        // Subscribe to user data stream
        if (!ws_manager_->subscribe_to_user_data()) {
            std::cout << "[BINANCE] Failed to subscribe to user data stream" << std::endl;
            return Result<bool>(ExchangeError("SUBSCRIPTION_ERROR", "Failed to subscribe to user data stream", "BINANCE", "connect"));
        }
        
        // Test connection with account info
        HttpResponse account_response = make_request("/fapi/v2/account", "GET", "", true);
        
        if (!account_response.success) {
            std::cout << "[BINANCE] Failed to connect - HTTP error: " << account_response.error_message << std::endl;
            return Result<bool>(ExchangeError("CONNECTION_FAILED", account_response.error_message, "BINANCE", "connect"));
        }
        
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(account_response.body, root)) {
            std::cout << "[BINANCE] Failed to parse account response" << std::endl;
            return Result<bool>(ExchangeError("PARSE_ERROR", "Failed to parse account response", "BINANCE", "connect"));
        }
        
        if (root.isMember("code") && root["code"].asInt() != 0) {
            std::cout << "[BINANCE] API error: " << root["msg"].asString() << std::endl;
            return Result<bool>(ExchangeError("API_ERROR", root["msg"].asString(), "BINANCE", "connect"));
        }
        
        // Setup WebSocket callbacks
        setup_websocket_callbacks();
        
        // Connect WebSocket
        std::string ws_url = config_.ws_url + "/ws/" + listen_key_;
        if (!websocket_handler_->connect(ws_url)) {
            std::cout << "[BINANCE] Failed to connect WebSocket" << std::endl;
            return Result<bool>(ExchangeError("WEBSOCKET_ERROR", "Failed to connect WebSocket", "BINANCE", "connect"));
        }
        
        connected_.store(true);
        running_.store(true);
        
        std::cout << "[BINANCE] Connected successfully" << std::endl;
        
        return Result<bool>(true);
        
    } catch (const std::exception& e) {
        std::cout << "[BINANCE] Connection failed: " << e.what() << std::endl;
        return Result<bool>(ExchangeError("CONNECTION_ERROR", e.what(), "BINANCE", "connect"));
    }
}

void BinanceOMS::disconnect() {
    if (!connected_.load()) {
        return;
    }
    
    std::cout << "[BINANCE] Disconnecting from Binance..." << std::endl;
    
    running_.store(false);
    connected_.store(false);
    
    if (websocket_handler_) {
        websocket_handler_->disconnect();
        websocket_handler_->shutdown();
    }
    
    if (http_handler_) {
        http_handler_->shutdown();
    }
    
    std::cout << "[BINANCE] Disconnected" << std::endl;
}

bool BinanceOMS::is_connected() const {
    return connected_.load();
}

std::string BinanceOMS::get_exchange_name() const {
    return config_.exchange_name;
}

std::vector<std::string> BinanceOMS::get_supported_symbols() const {
    return {"BTCUSDT", "ETHUSDT", "ADAUSDT", "DOTUSDT", "LINKUSDT"};
}

Result<std::map<std::string, std::string>> BinanceOMS::get_health_status() const {
    std::map<std::string, std::string> status;
    status["connected"] = connected_.load() ? "true" : "false";
    status["exchange"] = "BINANCE";
    status["api_key_set"] = config_.api_key.empty() ? "false" : "true";
    status["http_handler"] = http_handler_ && http_handler_->is_initialized() ? "initialized" : "not_initialized";
    status["websocket_handler"] = websocket_handler_ && websocket_handler_->is_connected() ? "connected" : "disconnected";
    return status;
}

Result<std::map<std::string, double>> BinanceOMS::get_performance_metrics() const {
    std::map<std::string, double> metrics;
    metrics["orders_sent"] = orders_.size();
    metrics["connection_uptime"] = connected_.load() ? 1.0 : 0.0;
    metrics["websocket_connected"] = ws_connected_.load() ? 1.0 : 0.0;
    return metrics;
}

Result<OrderResponse> BinanceOMS::send_order(const Order& order) {
    if (!connected_.load()) {
        std::cout << "[BINANCE] Cannot send order - not connected" << std::endl;
        return Result<OrderResponse>(ExchangeError("NOT_CONNECTED", "Not connected to exchange", "BINANCE", "send_order"));
    }
    
    if (!check_rate_limit()) {
        std::cout << "[BINANCE] Rate limit exceeded" << std::endl;
        return Result<OrderResponse>(ExchangeError("RATE_LIMIT_EXCEEDED", "Exchange rate limit exceeded", "BINANCE", "send_order"));
    }

    std::cout << "[BINANCE] Sending order: " << order.cl_ord_id 
              << " " << to_string(order.side) << " " << order.qty 
              << " " << order.symbol << " @ " << order.price << std::endl;
    
    try {
        // Get endpoint configuration
        auto endpoint_config = get_endpoint_config("place_order");
        if (endpoint_config.path.empty()) {
            return Result<std::string>(ExchangeError("CONFIG_ERROR", "Place order endpoint not configured", "BINANCE", "send_order"));
        }
        
        // Create order payload
        std::string payload = create_order_payload(order);
        
        // Make API call using configured endpoint
        HttpResponse response = make_request(endpoint_config.path, 
                                           exchange_config::ApiEndpointManager::http_method_to_string(endpoint_config.method), 
                                           payload, endpoint_config.requires_signature);
        
        if (!response.success) {
            return Result<OrderResponse>(ExchangeError("HTTP_ERROR", response.error_message, "BINANCE", "send_order"));
        }
        
        // Parse response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response.body, root)) {
            return Result<OrderResponse>(ExchangeError("PARSE_ERROR", "Failed to parse Binance response", "BINANCE", "send_order"));
        }
        
        // Check for API errors
        if (root.isMember("code") && root["code"].asInt() != 0) {
            std::string error_msg = root["msg"].asString();
            return Result<OrderResponse>(ExchangeError("API_ERROR", error_msg, "BINANCE", "send_order"));
        }
        
        // Create successful response
        OrderResponse order_response(order.cl_ord_id, std::to_string(root["orderId"].asUInt64()), "BINANCE", order.symbol);
        order_response.qty = order.qty;
        order_response.price = order.price;
        order_response.side = to_string(order.side);
        order_response.status = root["status"].asString();
        order_response.timestamp = std::chrono::system_clock::now();
        
        // Store order state
        OrderStateInfo order_info;
        order_info.cl_ord_id = order.cl_ord_id;
        order_info.exchange_order_id = std::to_string(root["orderId"].asUInt64());
        order_info.exch = "BINANCE";
        order_info.symbol = order.symbol;
        order_info.side = order.side;
        order_info.qty = order.qty;
        order_info.price = order.price;
        order_info.state = OrderState::ACKNOWLEDGED;
        
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_[order.cl_ord_id] = order_info;
        }
        
        // Update rate limit
        update_rate_limit();
        
        // Emit order event
        OrderEvent event;
        event.cl_ord_id = order.cl_ord_id;
        event.exchange_order_id = std::to_string(root["orderId"].asUInt64());
        event.exch = "BINANCE";
        event.symbol = order.symbol;
        event.type = OrderEventType::Ack;
        event.fill_qty = order.qty;
        event.fill_price = order.price;
        event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(order_response.timestamp.time_since_epoch()).count();
        
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (on_order_event) {
                on_order_event(event);
            }
        }
        
        std::cout << "[BINANCE] Order sent successfully: " << order.cl_ord_id 
                  << " -> " << order_info.exchange_order_id << std::endl;
        
        return Result<OrderResponse>(std::move(order_response));
        
    } catch (const std::exception& e) {
        std::cout << "[BINANCE] Error sending order: " << e.what() << std::endl;
        return Result<OrderResponse>(ExchangeError("EXCEPTION", e.what(), "BINANCE", "send_order"));
    }
}

Result<bool> BinanceOMS::cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) {
    if (!connected_.load()) {
        return Result<bool>(ExchangeError("NOT_CONNECTED", "Not connected to exchange", "BINANCE", "cancel_order"));
    }
    
    if (!check_rate_limit()) {
        return Result<bool>(ExchangeError("RATE_LIMIT_EXCEEDED", "Exchange rate limit exceeded", "BINANCE", "cancel_order"));
    }

    std::string target_exchange_order_id = exchange_order_id;
    if (target_exchange_order_id.empty()) {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(cl_ord_id);
        if (it != orders_.end()) {
            target_exchange_order_id = it->second.exchange_order_id;
        }
    }

    if (target_exchange_order_id.empty()) {
        return Result<bool>(ExchangeError("ORDER_NOT_FOUND", "Order not found", "BINANCE", "cancel_order"));
    }

    std::cout << "[BINANCE] Cancelling order: " << cl_ord_id << " (exchange ID: " << target_exchange_order_id << ")" << std::endl;
    
    try {
        // Create cancel request payload
        std::string payload = "symbol=BTCUSDT&orderId=" + target_exchange_order_id + 
                              "&timestamp=" + std::to_string(std::time(nullptr) * 1000);
        
        // Make API call to Binance using HTTP handler
        HttpResponse response = make_request("/fapi/v1/order", "DELETE", payload, true);
        
        if (!response.success) {
            return Result<bool>(ExchangeError("HTTP_ERROR", response.error_message, "BINANCE", "cancel_order"));
        }
        
        // Parse response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response.body, root)) {
            return Result<bool>(ExchangeError("PARSE_ERROR", "Failed to parse Binance response", "BINANCE", "cancel_order"));
        }
        
        // Check for API errors
        if (root.isMember("code") && root["code"].asInt() != 0) {
            std::string error_msg = root["msg"].asString();
            return Result<bool>(ExchangeError("API_ERROR", error_msg, "BINANCE", "cancel_order"));
        }
        
        // Update local order state
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(cl_ord_id);
            if (it != orders_.end()) {
                it->second.state = OrderState::CANCELLED;
            }
        }
        
        // Update rate limit
        update_rate_limit();
        
        // Emit cancel event
        OrderEvent event;
        event.cl_ord_id = cl_ord_id;
        event.exchange_order_id = target_exchange_order_id;
        event.exch = "BINANCE";
        event.symbol = "BTCUSDT"; // TODO: Get from order
        event.type = OrderEventType::Cancel;
        event.fill_qty = 0.0;
        event.fill_price = 0.0;
        event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (on_order_event) {
                on_order_event(event);
            }
        }
        
        std::cout << "[BINANCE] Order cancelled successfully: " << cl_ord_id << std::endl;
        
        return Result<bool>(true);
        
    } catch (const std::exception& e) {
        std::cout << "[BINANCE] Error cancelling order: " << e.what() << std::endl;
        return Result<bool>(ExchangeError("EXCEPTION", e.what(), "BINANCE", "cancel_order"));
    }
}

Result<bool> BinanceOMS::modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                                     double new_price, double new_qty) {
    if (!connected_.load()) {
        return Result<bool>(ExchangeError("NOT_CONNECTED", "Not connected to exchange", "BINANCE", "modify_order"));
    }
    
    std::cout << "[BINANCE] Modifying order: " << cl_ord_id 
              << " new_price=" << new_price << " new_qty=" << new_qty << std::endl;
    
    // Binance doesn't support order modification, need to cancel and replace
    auto cancel_result = cancel_order(cl_ord_id, exchange_order_id);
    if (!cancel_result.is_success()) {
        return cancel_result;
    }
    
    return Result<bool>(true);
}

// Configuration management implementation
void BinanceOMS::set_asset_type(exchange_config::AssetType asset_type) {
    current_asset_type_ = asset_type;
    asset_config_ = exchange_config::g_api_endpoint_manager->get_asset_config("BINANCE", asset_type);
    
    // Update data fetcher with new base URL
    if (data_fetcher_) {
        data_fetcher_->set_base_url(asset_config_.base_url);
    }
    
    std::cout << "[BINANCE] Switched to asset type: " 
              << exchange_config::ApiEndpointManager::asset_type_to_string(asset_type) << std::endl;
}

exchange_config::AssetType BinanceOMS::get_asset_type() const {
    return current_asset_type_;
}

exchange_config::AssetConfig BinanceOMS::get_asset_config() const {
    return asset_config_;
}

std::string BinanceOMS::get_endpoint_url(const std::string& endpoint_name) const {
    return exchange_config::get_api_endpoint("BINANCE", current_asset_type_, endpoint_name);
}

exchange_config::EndpointConfig BinanceOMS::get_endpoint_config(const std::string& endpoint_name) const {
    return exchange_config::get_endpoint_info("BINANCE", current_asset_type_, endpoint_name);
}

std::vector<BinanceOrder> BinanceOMS::get_order_history(const std::string& symbol, 
                                                       uint64_t start_time, 
                                                       uint64_t end_time) {
    if (!data_fetcher_) {
        return {};
    }
    return data_fetcher_->get_order_history(symbol, start_time, end_time);
}

std::vector<BinancePosition> BinanceOMS::get_positions() {
    if (!data_fetcher_) {
        return {};
    }
    return data_fetcher_->get_positions();
}

std::vector<BinanceTrade> BinanceOMS::get_trade_history(const std::string& symbol,
                                                       uint64_t start_time,
                                                       uint64_t end_time) {
    if (!data_fetcher_) {
        return {};
    }
    return data_fetcher_->get_trade_history(symbol, start_time, end_time);
}

std::vector<BinanceBalance> BinanceOMS::get_balances() {
    if (!data_fetcher_) {
        return {};
    }
    return data_fetcher_->get_balances();
}

void BinanceOMS::set_http_handler(std::unique_ptr<IHttpHandler> handler) {
    http_handler_ = std::move(handler);
}

void BinanceOMS::set_websocket_handler(std::unique_ptr<IWebSocketHandler> handler) {
    websocket_handler_ = std::move(handler);
}

HttpResponse BinanceOMS::make_request(const std::string& endpoint, const std::string& method, 
                                    const std::string& body, bool is_signed) {
    if (!http_handler_) {
        HttpResponse response;
        response.error_message = "HTTP handler not initialized";
        return response;
    }
    
    HttpRequest request;
    request.method = method;
    request.url = asset_config_.base_url + endpoint;  // Use configured base URL
    request.body = body;
    request.timeout_ms = config_.timeout_ms;
    
    // Add default headers from configuration
    for (const auto& [key, value] : asset_config_.headers) {
        request.headers[key] = value;
    }
    
    if (is_signed) {
        auto auth_headers = create_auth_headers(method, endpoint, body);
        request.headers.insert(auth_headers.begin(), auth_headers.end());
    }
    
    return http_handler_->make_request(request);
}

std::string BinanceOMS::create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) {
    std::map<std::string, std::string> headers;
    
    // Add API key header
    headers["X-MBX-APIKEY"] = config_.api_key;
    
    // Add signature to body
    std::string query_string = body.empty() ? "" : body;
    if (!query_string.empty() && query_string.find("timestamp=") == std::string::npos) {
        query_string += "&timestamp=" + std::to_string(std::time(nullptr) * 1000);
    }
    
    std::string signature = generate_signature(query_string);
    query_string += "&signature=" + signature;
    
    return headers;
}

std::string BinanceOMS::generate_signature(const std::string& data) {
    unsigned char* digest;
    unsigned int digest_len;
    
    digest = HMAC(EVP_sha256(), 
                  config_.api_secret.c_str(), config_.api_secret.length(),
                  (unsigned char*)data.c_str(), data.length(),
                  nullptr, &digest_len);
    
    char md_string[65];
    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(&md_string[i*2], "%02x", (unsigned int)digest[i]);
    }
    
    return std::string(md_string);
}

void BinanceOMS::setup_websocket_callbacks() {
    if (!websocket_handler_) return;
    
    websocket_handler_->set_message_callback([this](const WebSocketMessage& message) {
        handle_websocket_message(message.data);
    });
    
    websocket_handler_->set_error_callback([this](const std::string& error) {
        std::cout << "[BINANCE] WebSocket error: " << error << std::endl;
    });
    
    websocket_handler_->set_connect_callback([this](bool connected) {
        ws_connected_.store(connected);
        std::cout << "[BINANCE] WebSocket " << (connected ? "connected" : "disconnected") << std::endl;
    });
}

void BinanceOMS::handle_websocket_message(const std::string& message) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
        std::cout << "[BINANCE] Failed to parse WebSocket message" << std::endl;
        return;
    }
    
    // Handle different message types
    if (root.isMember("e")) {
        std::string event_type = root["e"].asString();
        
        if (event_type == "executionReport") {
            process_order_update(root);
        } else if (event_type == "outboundAccountPosition") {
            process_account_update(root);
        }
    }
}

void BinanceOMS::process_order_update(const Json::Value& data) {
    std::string client_order_id = data["c"].asString();
    std::string order_status = data["X"].asString();
    
    OrderState new_state = OrderState::ACKNOWLEDGED;
    if (order_status == "FILLED") {
        new_state = OrderState::FILLED;
    } else if (order_status == "CANCELED") {
        new_state = OrderState::CANCELLED;
    } else if (order_status == "REJECTED") {
        new_state = OrderState::REJECTED;
    }
    
    // Update order state
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(client_order_id);
        if (it != orders_.end()) {
            it->second.state = new_state;
        }
    }
    
    // Emit order event
    OrderEvent event;
    event.cl_ord_id = client_order_id;
    event.exchange_order_id = data["i"].asString();
    event.exch = "BINANCE";
    event.symbol = data["s"].asString();
    
    if (new_state == OrderState::FILLED) {
        event.type = OrderEventType::Fill;
        event.fill_qty = data["z"].asDouble();
        event.fill_price = data["ap"].asDouble();
    } else if (new_state == OrderState::CANCELLED) {
        event.type = OrderEventType::Cancel;
    } else if (new_state == OrderState::REJECTED) {
        event.type = OrderEventType::Reject;
    }
    
    event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (on_order_event) {
            on_order_event(event);
        }
    }
}

void BinanceOMS::process_account_update(const Json::Value& data) {
    // Handle account balance updates
    std::cout << "[BINANCE] Account update received" << std::endl;
}

std::string BinanceOMS::create_order_payload(const Order& order) {
    Json::Value payload;
    payload["symbol"] = order.symbol;
    payload["side"] = (order.side == Side::Buy) ? "BUY" : "SELL";
    payload["type"] = "LIMIT";
    payload["timeInForce"] = "GTC";
    payload["quantity"] = std::to_string(order.qty);
    payload["price"] = std::to_string(order.price);
    payload["newClientOrderId"] = order.cl_ord_id;
    payload["timestamp"] = std::to_string(std::time(nullptr) * 1000);
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, payload);
}

bool BinanceOMS::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    // Reset counter every minute
    if (now - last_reset_ > std::chrono::minutes(1)) {
        requests_per_minute_ = 0;
        last_reset_ = now;
    }
    
    // Binance allows 1200 requests per minute
    return requests_per_minute_ < 1200;
}

void BinanceOMS::update_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    requests_per_minute_++;
}

} // namespace binance
