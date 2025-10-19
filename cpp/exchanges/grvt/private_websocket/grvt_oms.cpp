#include "grvt_oms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace grvt {

GrvtOMS::GrvtOMS(const GrvtOMSConfig& config) : config_(config) {
    std::cout << "[GRVT_OMS] Initializing GRVT OMS" << std::endl;
}

GrvtOMS::~GrvtOMS() {
    disconnect();
}

bool GrvtOMS::connect() {
    std::cout << "[GRVT_OMS] Connecting to GRVT WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[GRVT_OMS] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&GrvtOMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[GRVT_OMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        std::cout << "[GRVT_OMS] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_OMS] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void GrvtOMS::disconnect() {
    std::cout << "[GRVT_OMS] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    connected_ = false;
    authenticated_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    std::cout << "[GRVT_OMS] Disconnected" << std::endl;
}

bool GrvtOMS::is_connected() const {
    return connected_.load();
}

void GrvtOMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    config_.session_cookie = secret; // GRVT uses session cookie instead of secret
    authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
}

bool GrvtOMS::is_authenticated() const {
    return authenticated_.load();
}

bool GrvtOMS::cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[GRVT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string cancel_msg = create_cancel_message(cl_ord_id, exch_ord_id);
    std::cout << "[GRVT_OMS] Sending cancel order: " << cancel_msg << std::endl;
    
    // Mock WebSocket send
    handle_websocket_message(R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"orderId":")" + exch_ord_id + R"(","status":"CANCELED"}})");
    
    return true;
}

bool GrvtOMS::replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[GRVT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string replace_msg = create_replace_message(cl_ord_id, new_order);
    std::cout << "[GRVT_OMS] Sending replace order: " << replace_msg << std::endl;
    
    // Mock WebSocket send
    handle_websocket_message(R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"orderId":")" + cl_ord_id + R"(","status":"REPLACED"}})");
    
    return true;
}

proto::OrderEvent GrvtOMS::get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id(cl_ord_id);
    order_event.set_exch("GRVT");
    order_event.set_exch_order_id(exch_ord_id);
    order_event.set_event_type(proto::OrderEventType::ACK);
    order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    return order_event;
}

bool GrvtOMS::place_market_order(const std::string& symbol, const std::string& side, double quantity) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[GRVT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, 0.0, "MARKET");
    std::cout << "[GRVT_OMS] Sending market order: " << order_msg << std::endl;
    
    // Mock WebSocket send
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"orderId":")" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(","status":"NEW","symbol":")" + symbol + R"(","side":")" + side + R"(","quantity":)" + std::to_string(quantity) + R"(}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool GrvtOMS::place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[GRVT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, price, "LIMIT");
    std::cout << "[GRVT_OMS] Sending limit order: " << order_msg << std::endl;
    
    // Mock WebSocket send
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"orderId":")" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(","status":"NEW","symbol":")" + symbol + R"(","side":")" + side + R"(","quantity":)" + std::to_string(quantity) + R"(,"price":)" + std::to_string(price) + R"(}})";
    handle_websocket_message(mock_response);
    
    return true;
}

void GrvtOMS::set_order_status_callback(OrderStatusCallback callback) {
    order_status_callback_ = callback;
}

void GrvtOMS::websocket_loop() {
    std::cout << "[GRVT_OMS] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional order updates
            static int counter = 0;
            if (++counter % 50 == 0) {
                std::string mock_order_update = R"({"jsonrpc":"2.0","method":"order_update","params":{"orderId":")" + std::to_string(counter) + R"(","status":"FILLED","symbol":"BTCUSDT","side":"BUY","quantity":0.1,"price":50000}})";
                handle_websocket_message(mock_order_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[GRVT_OMS] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[GRVT_OMS] WebSocket loop stopped" << std::endl;
}

void GrvtOMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[GRVT_OMS] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "order_update" && root.isMember("params")) {
                handle_order_update(root["params"]);
            } else if (method == "trade_update" && root.isMember("params")) {
                handle_trade_update(root["params"]);
            }
        } else if (root.isMember("result")) {
            // Handle order response
            std::cout << "[GRVT_OMS] Order response: " << message << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_OMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void GrvtOMS::handle_order_update(const Json::Value& order_data) {
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id(order_data["orderId"].asString());
    order_event.set_exch("GRVT");
    order_event.set_symbol(order_data["symbol"].asString());
    order_event.set_exch_order_id(order_data["orderId"].asString());
    order_event.set_fill_qty(order_data["quantity"].asDouble());
    order_event.set_fill_price(order_data["price"].asDouble());
    order_event.set_event_type(map_order_status(order_data["status"].asString()));
    order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    if (order_status_callback_) {
        order_status_callback_(order_event);
    }
    
    std::cout << "[GRVT_OMS] Order update: " << order_event.cl_ord_id() 
              << " status: " << order_data["status"].asString() << std::endl;
}

void GrvtOMS::handle_trade_update(const Json::Value& trade_data) {
    std::cout << "[GRVT_OMS] Trade update: " << trade_data.toStyledString() << std::endl;
}

std::string GrvtOMS::create_order_message(const std::string& symbol, const std::string& side, 
                                        double quantity, double price, const std::string& order_type) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "place_order";
    
    Json::Value params;
    params["symbol"] = symbol;
    params["side"] = map_side_to_grvt(side);
    params["quantity"] = quantity;
    params["type"] = map_order_type_to_grvt(order_type);
    if (price > 0) {
        params["price"] = price;
    }
    params["timeInForce"] = "GTC";
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtOMS::create_cancel_message(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "cancel_order";
    
    Json::Value params;
    params["orderId"] = exch_ord_id;
    params["clientOrderId"] = cl_ord_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtOMS::create_replace_message(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "replace_order";
    
    Json::Value params;
    params["orderId"] = cl_ord_id;
    params["symbol"] = new_order.symbol();
    params["side"] = map_side_to_grvt(new_order.side() == proto::Side::BUY ? "BUY" : "SELL");
    params["quantity"] = new_order.qty();
    params["price"] = new_order.price();
    params["type"] = map_order_type_to_grvt(new_order.type() == proto::OrderType::MARKET ? "MARKET" : "LIMIT");
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

bool GrvtOMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    std::cout << "[GRVT_OMS] Authenticating: " << auth_msg << std::endl;
    
    // Mock authentication response
    std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"authenticated":true}})";
    handle_websocket_message(mock_auth_response);
    
    return true;
}

std::string GrvtOMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "auth";
    
    Json::Value params;
    params["apiKey"] = config_.api_key;
    params["sessionCookie"] = config_.session_cookie;
    params["accountId"] = config_.account_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtOMS::generate_request_id() {
    return std::to_string(request_id_++);
}

proto::OrderEventType GrvtOMS::map_order_status(const std::string& status) {
    if (status == "NEW") {
        return proto::OrderEventType::ACK;
    } else if (status == "FILLED") {
        return proto::OrderEventType::FILL;
    } else if (status == "CANCELED" || status == "CANCELLED") {
        return proto::OrderEventType::CANCEL;
    } else if (status == "REJECTED") {
        return proto::OrderEventType::REJECT;
    } else {
        return proto::OrderEventType::ACK;
    }
}

std::string GrvtOMS::map_side_to_grvt(const std::string& side) {
    if (side == "BUY") {
        return "BUY";
    } else if (side == "SELL") {
        return "SELL";
    }
    return side;
}

std::string GrvtOMS::map_order_type_to_grvt(const std::string& order_type) {
    if (order_type == "MARKET") {
        return "MARKET";
    } else if (order_type == "LIMIT") {
        return "LIMIT";
    }
    return order_type;
}

} // namespace grvt