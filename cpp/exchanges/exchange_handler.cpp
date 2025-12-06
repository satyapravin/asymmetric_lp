#include "exchange_handler.hpp"
#include "../utils/logging/log_helper.hpp"
#include <sstream>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <json/json.h>
#include <string>
#include <memory>
#include <thread>
#include <ctime>

// ExchangeHandler implementation
ExchangeHandler::ExchangeHandler(const ExchangeConfig& config)
    : config_(config) {
    // Initialize default handlers
    http_handler_ = HttpHandlerFactory::create("CURL");
    websocket_handler_ = WebSocketHandlerFactory::create("LIBUV");
}

ExchangeHandler::~ExchangeHandler() {
    stop();
}

bool ExchangeHandler::start() {
    if (running_.load()) return true;
    
    LOG_INFO_COMP("EXCHANGE_HANDLER", "Starting " + config_.name + " handler");
    
    // Initialize HTTP handler
    if (!http_handler_->initialize()) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Failed to initialize HTTP handler");
        return false;
    }
    
    // Initialize WebSocket handler
    if (!websocket_handler_->initialize()) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Failed to initialize WebSocket handler");
        return false;
    }
    
    // Set WebSocket callbacks
    websocket_handler_->set_message_callback([this](const WebSocketMessage& message) {
        handle_websocket_message(message.data);
    });
    
    websocket_handler_->set_error_callback([this](const std::string& error) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "WebSocket error: " + error);
    });
    
    websocket_handler_->set_connect_callback([this](bool connected) {
        connected_.store(connected);
        LOG_INFO_COMP("EXCHANGE_HANDLER", "WebSocket " + std::string(connected ? "connected" : "disconnected"));
    });
    
    // Connect WebSocket
    if (!websocket_handler_->connect(config_.websocket_url)) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Failed to connect WebSocket");
        return false;
    }
    
    running_.store(true);
    connected_.store(true);
    
    LOG_INFO_COMP("EXCHANGE_HANDLER", "Started successfully");
    return true;
}

void ExchangeHandler::stop() {
    if (!running_.load()) return;
    
    LOG_INFO_COMP("EXCHANGE_HANDLER", "Stopping " + config_.name + " handler");
    
    running_.store(false);
    connected_.store(false);
    
    if (websocket_handler_) {
        websocket_handler_->disconnect();
        websocket_handler_->shutdown();
    }
    
    if (http_handler_) {
        http_handler_->shutdown();
    }
    
    LOG_INFO_COMP("EXCHANGE_HANDLER", "Stopped");
}

bool ExchangeHandler::send_order(const Order& order) {
    if (!connected_.load()) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Not connected - cannot send order");
        return false;
    }
    
    std::string side_str = (order.side == OrderSide::BUY ? "BUY" : "SELL");
    std::string log_msg = "Sending order: " + order.client_order_id + " " + side_str + 
                          " " + std::to_string(order.quantity) + " @ " + std::to_string(order.price);
    LOG_DEBUG_COMP("EXCHANGE_HANDLER", log_msg);
    
    try {
        // Create order payload
        std::string payload = create_order_payload(order);
        
        // Make HTTP request
        HttpResponse response = make_http_request("POST", "/fapi/v1/order", payload, true);
        
        if (!response.success) {
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "Order failed: " + response.error_message);
            return false;
        }
        
        // Parse response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response.body, root)) {
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "Failed to parse order response");
            return false;
        }
        
        // Check for API errors
        if (root.isMember("code") && root["code"].asInt() != 0) {
            std::string error_msg = root["msg"].asString();
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "API error: " + error_msg);
            return false;
        }
        
        // Update order with exchange ID
        Order updated_order = order;
        updated_order.exchange_order_id = std::to_string(root["orderId"].asUInt64());
        updated_order.status = OrderStatus::PENDING;
        updated_order.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Store order
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            active_orders_[order.client_order_id] = updated_order;
        }
        
        // Notify callback
        if (order_event_callback_) {
            order_event_callback_(updated_order);
        }
        
        LOG_DEBUG_COMP("EXCHANGE_HANDLER", "Order sent successfully: " + order.client_order_id + 
                      " -> " + updated_order.exchange_order_id);
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Error sending order: " + std::string(e.what()));
        return false;
    }
}

bool ExchangeHandler::cancel_order(const std::string& client_order_id) {
    if (!connected_.load()) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Not connected - cannot cancel order");
        return false;
    }
    
    LOG_DEBUG_COMP("EXCHANGE_HANDLER", "Cancelling order: " + client_order_id);
    
    try {
        // Create cancel payload
        std::string payload = create_cancel_payload(client_order_id);
        
        // Make HTTP request
        HttpResponse response = make_http_request("DELETE", "/fapi/v1/order", payload, true);
        
        if (!response.success) {
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "Cancel failed: " + response.error_message);
            return false;
        }
        
        // Parse response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response.body, root)) {
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "Failed to parse cancel response");
            return false;
        }
        
        // Check for API errors
        if (root.isMember("code") && root["code"].asInt() != 0) {
            std::string error_msg = root["msg"].asString();
            LOG_ERROR_COMP("EXCHANGE_HANDLER", "API error: " + error_msg);
            return false;
        }
        
        // Update order status
        update_order_status(client_order_id, OrderStatus::CANCELLED);
        
        LOG_DEBUG_COMP("EXCHANGE_HANDLER", "Order cancelled successfully: " + client_order_id);
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("EXCHANGE_HANDLER", "Error cancelling order: " + std::string(e.what()));
        return false;
    }
}

bool ExchangeHandler::modify_order(const Order& order) {
    // Most exchanges don't support order modification, need to cancel and replace
    if (!cancel_order(order.client_order_id)) {
        return false;
    }
    
    // Small delay before sending new order
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return send_order(order);
}

std::vector<Order> ExchangeHandler::get_open_orders() {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    std::vector<Order> open_orders;
    
    for (const auto& [client_id, order] : active_orders_) {
        if (order.status == OrderStatus::PENDING) {
            open_orders.push_back(order);
        }
    }
    
    return open_orders;
}

Order ExchangeHandler::get_order_status(const std::string& client_order_id) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(client_order_id);
    if (it != active_orders_.end()) {
        return it->second;
    }
    
    // Return empty order if not found
    return Order{};
}

void ExchangeHandler::set_http_handler(std::unique_ptr<IHttpHandler> handler) {
    http_handler_ = std::move(handler);
}

void ExchangeHandler::set_websocket_handler(std::unique_ptr<IWebSocketHandler> handler) {
    websocket_handler_ = std::move(handler);
}

HttpResponse ExchangeHandler::make_http_request(const std::string& method, const std::string& endpoint, 
                                              const std::string& body, bool authenticated) {
    HttpRequest request;
    request.method = method;
    request.url = config_.base_url + endpoint;
    request.body = body;
    request.timeout_ms = config_.timeout_ms;
    
    if (authenticated) {
        request.headers = create_auth_headers(method, endpoint, body);
    }
    
    return http_handler_->make_request(request);
}

bool ExchangeHandler::send_websocket_message(const std::string& message) {
    return websocket_handler_->send_message(message);
}

void ExchangeHandler::update_order_status(const std::string& client_order_id, OrderStatus status, 
                                         double filled_qty, double avg_price) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(client_order_id);
    if (it != active_orders_.end()) {
        it->second.status = status;
        if (filled_qty > 0) {
            it->second.filled_quantity = filled_qty;
        }
        if (avg_price > 0) {
            it->second.average_price = avg_price;
        }
        
        // Notify callback
        if (order_event_callback_) {
            order_event_callback_(it->second);
        }
    }
}

// BinanceHandler implementation
BinanceHandler::BinanceHandler(const ExchangeConfig& config)
    : ExchangeHandler(config) {
    // Set Binance-specific configuration
    config_.base_url = config.testnet_mode ? "https://testnet.binance.vision" : "https://api.binance.com";
    config_.websocket_url = config.testnet_mode ? "wss://testnet.binance.vision" : "wss://stream.binance.com:9443";
}

std::string BinanceHandler::create_order_payload(const Order& order) {
    Json::Value payload;
    payload["symbol"] = order.symbol;
    payload["side"] = (order.side == OrderSide::BUY) ? "BUY" : "SELL";
    payload["type"] = "LIMIT";
    payload["timeInForce"] = "GTC";
    payload["quantity"] = std::to_string(order.quantity);
    payload["price"] = std::to_string(order.price);
    payload["newClientOrderId"] = order.client_order_id;
    payload["timestamp"] = std::to_string(std::time(nullptr) * 1000);
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, payload);
}

std::string BinanceHandler::create_cancel_payload(const std::string& client_order_id) {
    std::stringstream payload;
    payload << "symbol=BTCUSDT&origClientOrderId=" << client_order_id 
            << "&timestamp=" << std::time(nullptr) * 1000;
    return payload.str();
}

std::string BinanceHandler::create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) {
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

void BinanceHandler::handle_websocket_message(const std::string& message) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
        LOG_ERROR_COMP("BINANCE_HANDLER", "Failed to parse WebSocket message");
        return;
    }
    
    // Handle different message types
    if (root.isMember("e")) {
        std::string event_type = root["e"].asString();
        
        if (event_type == "executionReport") {
            handle_order_update(message);
        } else if (event_type == "outboundAccountPosition") {
            handle_account_update(message);
        }
    }
}

void BinanceHandler::handle_order_update(const std::string& message) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
        return;
    }
    
    std::string client_order_id = root["c"].asString();
    std::string order_status = root["X"].asString();
    
    OrderStatus status = OrderStatus::PENDING;
    if (order_status == "FILLED") {
        status = OrderStatus::FILLED;
    } else if (order_status == "CANCELED") {
        status = OrderStatus::CANCELLED;
    } else if (order_status == "REJECTED") {
        status = OrderStatus::REJECTED;
    }
    
    double filled_qty = root["z"].asDouble();
    double avg_price = root["ap"].asDouble();
    
    update_order_status(client_order_id, status, filled_qty, avg_price);
}

void BinanceHandler::handle_account_update(const std::string& message) {
    // Handle account balance updates
    LOG_INFO_COMP("BINANCE_HANDLER", "Account update received");
}

std::string BinanceHandler::generate_signature(const std::string& data) {
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

std::string BinanceHandler::create_listen_key() {
    HttpResponse response = make_http_request("POST", "/fapi/v1/listenKey", "", true);
    
    if (!response.success) {
        LOG_ERROR_COMP("BINANCE_HANDLER", "Failed to create listen key");
        return "";
    }
    
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(response.body, root)) {
        LOG_ERROR_COMP("BINANCE_HANDLER", "Failed to parse listen key response");
        return "";
    }
    
    return root["listenKey"].asString();
}

void BinanceHandler::refresh_listen_key() {
    refresh_running_.store(true);
    listen_key_refresh_thread_ = std::thread([this]() {
        while (refresh_running_.load()) {
            std::this_thread::sleep_for(std::chrono::minutes(30));
            
            if (refresh_running_.load()) {
                HttpResponse response = make_http_request("PUT", "/fapi/v1/listenKey", "", true);
                if (!response.success) {
                    LOG_ERROR_COMP("BINANCE_HANDLER", "Failed to refresh listen key");
                }
            }
        }
    });
}
