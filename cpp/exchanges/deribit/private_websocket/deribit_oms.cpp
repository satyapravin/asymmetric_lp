#include "deribit_oms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace deribit {

DeribitOMS::DeribitOMS(const DeribitOMSConfig& config) : config_(config) {
    std::cout << "[DERIBIT_OMS] Initializing Deribit OMS" << std::endl;
}

DeribitOMS::~DeribitOMS() {
    disconnect();
}

bool DeribitOMS::connect() {
    std::cout << "[DERIBIT_OMS] Connecting to Deribit WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[DERIBIT_OMS] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitOMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[DERIBIT_OMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        std::cout << "[DERIBIT_OMS] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_OMS] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void DeribitOMS::disconnect() {
    std::cout << "[DERIBIT_OMS] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    connected_ = false;
    authenticated_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    std::cout << "[DERIBIT_OMS] Disconnected" << std::endl;
}

bool DeribitOMS::is_connected() const {
    return connected_.load();
}

void DeribitOMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.client_id = api_key;
    config_.client_secret = secret;
    authenticated_.store(!config_.client_id.empty() && !config_.client_secret.empty());
}

bool DeribitOMS::is_authenticated() const {
    return authenticated_.load();
}

bool DeribitOMS::cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[DERIBIT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string cancel_msg = create_cancel_message(cl_ord_id, exch_ord_id);
    std::cout << "[DERIBIT_OMS] Sending cancel order: " << cancel_msg << std::endl;
    
    // Mock WebSocket send
    handle_websocket_message(R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"order_id":")" + exch_ord_id + R"(","order_state":"cancelled"}})");
    
    return true;
}

bool DeribitOMS::replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[DERIBIT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string replace_msg = create_replace_message(cl_ord_id, new_order);
    std::cout << "[DERIBIT_OMS] Sending replace order: " << replace_msg << std::endl;
    
    // Mock WebSocket send
    handle_websocket_message(R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"order_id":")" + cl_ord_id + R"(","order_state":"replaced"}})");
    
    return true;
}

proto::OrderEvent DeribitOMS::get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id(cl_ord_id);
    order_event.set_exch("DERIBIT");
    order_event.set_exch_order_id(exch_ord_id);
    order_event.set_event_type(proto::OrderEventType::ACK);
    order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    return order_event;
}

bool DeribitOMS::place_market_order(const std::string& symbol, const std::string& side, double quantity) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[DERIBIT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, 0.0, "MARKET");
    std::cout << "[DERIBIT_OMS] Sending market order: " << order_msg << std::endl;
    
    // Mock WebSocket send
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"order_id":")" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(","order_state":"open","instrument_name":")" + symbol + R"(","direction":")" + side + R"(","amount":)" + std::to_string(quantity) + R"(}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool DeribitOMS::place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) {
    if (!is_connected() || !is_authenticated()) {
        std::cerr << "[DERIBIT_OMS] Not connected or authenticated" << std::endl;
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, price, "LIMIT");
    std::cout << "[DERIBIT_OMS] Sending limit order: " << order_msg << std::endl;
    
    // Mock WebSocket send
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"order_id":")" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(","order_state":"open","instrument_name":")" + symbol + R"(","direction":")" + side + R"(","amount":)" + std::to_string(quantity) + R"(,"price":)" + std::to_string(price) + R"(}})";
    handle_websocket_message(mock_response);
    
    return true;
}

void DeribitOMS::set_order_status_callback(OrderStatusCallback callback) {
    order_status_callback_ = callback;
}

void DeribitOMS::websocket_loop() {
    std::cout << "[DERIBIT_OMS] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional order updates
            static int counter = 0;
            if (++counter % 50 == 0) {
                std::string mock_order_update = R"({"jsonrpc":"2.0","method":"user.order","params":{"order_id":")" + std::to_string(counter) + R"(","order_state":"filled","instrument_name":"BTC-PERPETUAL","direction":"buy","amount":0.1,"price":50000}})";
                handle_websocket_message(mock_order_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DERIBIT_OMS] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[DERIBIT_OMS] WebSocket loop stopped" << std::endl;
}

void DeribitOMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[DERIBIT_OMS] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "user.order" && root.isMember("params")) {
                handle_order_update(root["params"]);
            } else if (method == "user.trades" && root.isMember("params")) {
                handle_trade_update(root["params"]);
            }
        } else if (root.isMember("result")) {
            // Handle order response
            std::cout << "[DERIBIT_OMS] Order response: " << message << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_OMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void DeribitOMS::handle_order_update(const Json::Value& order_data) {
    proto::OrderEvent order_event;
    order_event.set_cl_ord_id(order_data["order_id"].asString());
    order_event.set_exch("DERIBIT");
    order_event.set_symbol(order_data["instrument_name"].asString());
    order_event.set_exch_order_id(order_data["order_id"].asString());
    order_event.set_fill_qty(order_data["amount"].asDouble());
    order_event.set_fill_price(order_data["price"].asDouble());
    order_event.set_event_type(map_order_status(order_data["order_state"].asString()));
    order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    if (order_status_callback_) {
        order_status_callback_(order_event);
    }
    
    std::cout << "[DERIBIT_OMS] Order update: " << order_event.cl_ord_id() 
              << " status: " << order_data["order_state"].asString() << std::endl;
}

void DeribitOMS::handle_trade_update(const Json::Value& trade_data) {
    std::cout << "[DERIBIT_OMS] Trade update: " << trade_data.toStyledString() << std::endl;
}

std::string DeribitOMS::create_order_message(const std::string& symbol, const std::string& side, 
                                            double quantity, double price, const std::string& order_type) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "private/buy";
    if (side == "SELL") {
        root["method"] = "private/sell";
    }
    
    Json::Value params;
    params["instrument_name"] = symbol;
    params["amount"] = quantity;
    params["type"] = map_order_type_to_deribit(order_type);
    if (price > 0) {
        params["price"] = price;
    }
    params["time_in_force"] = "good_til_cancelled";
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitOMS::create_cancel_message(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "private/cancel";
    
    Json::Value params;
    params["order_id"] = exch_ord_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitOMS::create_replace_message(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "private/edit";
    
    Json::Value params;
    params["order_id"] = cl_ord_id;
    params["instrument_name"] = new_order.symbol();
    params["amount"] = new_order.qty();
    params["price"] = new_order.price();
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

bool DeribitOMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    std::cout << "[DERIBIT_OMS] Authenticating: " << auth_msg << std::endl;
    
    // Mock authentication response
    std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"access_token":")" + get_access_token() + R"(","expires_in":3600}})";
    handle_websocket_message(mock_auth_response);
    
    return true;
}

std::string DeribitOMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "public/auth";
    
    Json::Value params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitOMS::get_access_token() {
    // Mock access token generation
    return "mock_access_token_" + std::to_string(std::time(nullptr));
}

std::string DeribitOMS::generate_request_id() {
    return std::to_string(request_id_++);
}

proto::OrderEventType DeribitOMS::map_order_status(const std::string& status) {
    if (status == "open") {
        return proto::OrderEventType::ACK;
    } else if (status == "filled") {
        return proto::OrderEventType::FILL;
    } else if (status == "cancelled") {
        return proto::OrderEventType::CANCEL;
    } else if (status == "rejected") {
        return proto::OrderEventType::REJECT;
    } else {
        return proto::OrderEventType::ACK;
    }
}

std::string DeribitOMS::map_side_to_deribit(const std::string& side) {
    if (side == "BUY") {
        return "buy";
    } else if (side == "SELL") {
        return "sell";
    }
    return side;
}

std::string DeribitOMS::map_order_type_to_deribit(const std::string& order_type) {
    if (order_type == "MARKET") {
        return "market";
    } else if (order_type == "LIMIT") {
        return "limit";
    }
    return order_type;
}

} // namespace deribit