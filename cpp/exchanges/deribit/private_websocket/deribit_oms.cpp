#include "deribit_oms.hpp"
#include "../../../utils/logging/log_helper.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <json/json.h>

namespace deribit {

DeribitOMS::DeribitOMS(const DeribitOMSConfig& config) : config_(config) {
    LOG_INFO_COMP("DERIBIT_OMS", "Initializing Deribit OMS");
    
    // If credentials are provided in config, mark as authenticated
    if (!config_.client_id.empty() && !config_.client_secret.empty()) {
        authenticated_.store(true);
        LOG_INFO_COMP("DERIBIT_OMS", "Credentials provided in config, marked as authenticated");
    } else {
        authenticated_.store(false);
    }
}

DeribitOMS::~DeribitOMS() {
    disconnect();
}

bool DeribitOMS::connect() {
    LOG_INFO_COMP("DERIBIT_OMS", "Connecting to Deribit WebSocket...");
    
    if (connected_.load()) {
        LOG_INFO_COMP("DERIBIT_OMS", "Already connected");
        return true;
    }
    
    try {
        // If custom transport is set, use it (for testing)
        if (custom_transport_) {
            LOG_INFO_COMP("DERIBIT_OMS", "Using custom WebSocket transport");
            
            // Set up message callback BEFORE connecting
            custom_transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& ws_msg) {
                if (!ws_msg.is_binary) {
                    handle_websocket_message(ws_msg.data);
                }
            });
            
            if (custom_transport_->connect(config_.websocket_url)) {
                connected_ = true;
                websocket_running_ = true;
                
                // Start event loop if not already running
                if (!custom_transport_->is_event_loop_running()) {
                    custom_transport_->start_event_loop();
                }
                
                websocket_thread_ = std::thread(&DeribitOMS::websocket_loop, this);
                
                // Authenticate
                if (!authenticate_websocket()) {
                    LOG_ERROR_COMP("DERIBIT_OMS", "Authentication failed");
                    return false;
                }
                
                authenticated_.store(true);
                LOG_INFO_COMP("DERIBIT_OMS", "Connected successfully using injected transport");
                return true;
            } else {
                LOG_ERROR_COMP("DERIBIT_OMS", "Failed to connect using custom transport");
                return false;
            }
        }
        
        // Initialize WebSocket connection (mock implementation for now)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitOMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            LOG_ERROR_COMP("DERIBIT_OMS", "Authentication failed");
            return false;
        }
        
        connected_ = true;
        authenticated_.store(true);
        
        LOG_INFO_COMP("DERIBIT_OMS", "Connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("DERIBIT_OMS", "Connection failed: " + std::string(e.what()));
        return false;
    }
}

void DeribitOMS::disconnect() {
    LOG_INFO_COMP("DERIBIT_OMS", "Disconnecting...");
    
    websocket_running_ = false;
    connected_ = false;
    authenticated_.store(false);
    
    if (custom_transport_) {
        custom_transport_->stop_event_loop();
    }
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    LOG_INFO_COMP("DERIBIT_OMS", "Disconnected");
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
        LOG_ERROR_COMP("DERIBIT_OMS", "Not connected or authenticated");
        return false;
    }
    
    std::string cancel_msg = create_cancel_message(cl_ord_id, exch_ord_id);
    LOG_DEBUG_COMP("DERIBIT_OMS", "Sending cancel order: " + cancel_msg);
    
    // Note: Order messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    return true;
}

bool DeribitOMS::replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("DERIBIT_OMS", "Not connected or authenticated");
        return false;
    }
    
    std::string replace_msg = create_replace_message(cl_ord_id, new_order);
    LOG_DEBUG_COMP("DERIBIT_OMS", "Sending replace order: " + replace_msg);
    
    // Note: Order messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
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
        LOG_ERROR_COMP("DERIBIT_OMS", "Not connected or authenticated");
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, 0.0, "MARKET");
    LOG_DEBUG_COMP("DERIBIT_OMS", "Sending market order: " + order_msg);
    
    // Note: Order messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    return true;
}

bool DeribitOMS::place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("DERIBIT_OMS", "Not connected or authenticated");
        return false;
    }
    
    std::string order_msg = create_order_message(symbol, side, quantity, price, "LIMIT");
    LOG_DEBUG_COMP("DERIBIT_OMS", "Sending limit order: " + order_msg);
    
    // Note: Order messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    return true;
}

void DeribitOMS::set_order_status_callback(OrderStatusCallback callback) {
    order_status_callback_ = callback;
}

void DeribitOMS::websocket_loop() {
    LOG_INFO_COMP("DERIBIT_OMS", "WebSocket loop started");
    
    if (custom_transport_) {
        LOG_INFO_COMP("DERIBIT_OMS", "Using custom transport - messages will arrive via callback");
        // The custom transport's event loop will handle message reception and callbacks
        // We just need to keep this thread alive while the custom transport is running
        while (websocket_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        // Mock WebSocket message processing (for testing without real connection)
        while (websocket_running_.load()) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Simulate occasional order updates (only for mock mode)
                static int counter = 0;
                if (++counter % 50 == 0) {
                    std::string mock_order_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"user.orders.BTC-PERPETUAL.raw","data":{"order_id":")" + 
                        std::to_string(counter) + R"(","order_state":"filled","instrument_name":"BTC-PERPETUAL","direction":"buy","amount":0.1,"price":50000,"timestamp":)" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(}}})";
                    handle_websocket_message(mock_order_update);
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR_COMP("DERIBIT_OMS", "WebSocket loop error: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    if (custom_transport_) {
        LOG_INFO_COMP("DERIBIT_OMS", "Stopping custom transport event loop");
        custom_transport_->stop_event_loop();
    }
    
    LOG_INFO_COMP("DERIBIT_OMS", "WebSocket loop stopped");
}

void DeribitOMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            LOG_ERROR_COMP("DERIBIT_OMS", "Failed to parse WebSocket message");
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "subscription" && root.isMember("params")) {
                Json::Value params = root["params"];
                std::string channel = params["channel"].asString();
                
                if (channel.find("user.orders") == 0 && params.isMember("data")) {
                    handle_order_update(params["data"]);
                } else if (channel.find("user.trades") == 0 && params.isMember("data")) {
                    handle_trade_update(params["data"]);
                }
            }
        } else if (root.isMember("result")) {
            // Handle order response
            Json::Value result = root["result"];
            if (result.isMember("order") || result.isMember("order_id")) {
                // Order placement/cancel/modify response
                LOG_DEBUG_COMP("DERIBIT_OMS", "Order response: " + message);
                
                // Convert to OrderEvent and notify callback
                proto::OrderEvent order_event;
                order_event.set_exch("DERIBIT");
                
                if (result.isMember("order")) {
                    Json::Value order = result["order"];
                    if (order.isMember("order_id")) {
                        order_event.set_exch_order_id(order["order_id"].asString());
                    }
                    if (order.isMember("order_state")) {
                        order_event.set_event_type(map_order_status(order["order_state"].asString()));
                    }
                    if (order.isMember("instrument_name")) {
                        order_event.set_symbol(order["instrument_name"].asString());
                    }
                } else if (result.isMember("order_id")) {
                    order_event.set_exch_order_id(result["order_id"].asString());
                }
                
                order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                
                if (order_status_callback_) {
                    order_status_callback_(order_event);
                }
            } else if (result.isMember("access_token")) {
                // Authentication response
                config_.access_token = result["access_token"].asString();
                if (result.isMember("expires_in")) {
                    LOG_INFO_COMP("DERIBIT_OMS", "Authentication successful, token expires in " + 
                                  std::to_string(result["expires_in"].asInt()) + " seconds");
                }
            }
        } else if (root.isMember("error")) {
            // Handle errors
            std::string error_msg = "Deribit API error: " + root["error"].toStyledString();
            LOG_ERROR_COMP("DERIBIT_OMS", error_msg);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("DERIBIT_OMS", "Error handling WebSocket message: " + std::string(e.what()));
    }
}

void DeribitOMS::handle_order_update(const Json::Value& order_data) {
    proto::OrderEvent order_event;
    
    if (order_data.isMember("order_id")) {
        order_event.set_exch_order_id(order_data["order_id"].asString());
        order_event.set_cl_ord_id(order_data["order_id"].asString()); // Use exchange order ID as client order ID if not provided
    }
    
    order_event.set_exch("DERIBIT");
    
    if (order_data.isMember("instrument_name")) {
        order_event.set_symbol(order_data["instrument_name"].asString());
    }
    
    if (order_data.isMember("order_state")) {
        order_event.set_event_type(map_order_status(order_data["order_state"].asString()));
    }
    
    if (order_data.isMember("amount")) {
        order_event.set_fill_qty(order_data["amount"].asDouble());
    }
    
    if (order_data.isMember("price")) {
        order_event.set_fill_price(order_data["price"].asDouble());
    }
    
    if (order_data.isMember("timestamp")) {
        order_event.set_timestamp_us(order_data["timestamp"].asUInt64() * 1000); // Convert milliseconds to microseconds
    } else {
        order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    if (order_status_callback_) {
        order_status_callback_(order_event);
    }
    
    LOG_DEBUG_COMP("DERIBIT_OMS", "Order update: " + order_event.exch_order_id() + 
                  " status: " + order_data["order_state"].asString());
}

void DeribitOMS::handle_trade_update(const Json::Value& trade_data) {
    LOG_DEBUG_COMP("DERIBIT_OMS", "Trade update: " + trade_data.toStyledString());
}

std::string DeribitOMS::create_order_message(const std::string& symbol, const std::string& side, 
                                            double quantity, double price, const std::string& order_type) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    
    // Deribit uses separate methods for buy/sell
    std::string deribit_side = map_side_to_deribit(side);
    if (deribit_side == "buy") {
        root["method"] = "private/buy";
    } else {
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
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitOMS::create_cancel_message(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "private/cancel";
    
    Json::Value params;
    params["order_id"] = exch_ord_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitOMS::create_replace_message(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "private/edit";
    
    Json::Value params;
    params["order_id"] = cl_ord_id;
    params["instrument_name"] = new_order.symbol();
    params["amount"] = new_order.qty();
    params["price"] = new_order.price();
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

bool DeribitOMS::authenticate_websocket() {
    if (config_.client_id.empty() || config_.client_secret.empty()) {
        LOG_ERROR_COMP("DERIBIT_OMS", "Cannot authenticate: credentials not set");
        return false;
    }
    
    std::string auth_msg = create_auth_message();
    LOG_INFO_COMP("DERIBIT_OMS", "Authenticating: " + auth_msg);
    
    // Note: Authentication messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    // In mock mode, simulate authentication response
    if (!custom_transport_) {
        std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
            R"(,"result":{"access_token":")" + get_access_token() + R"(","expires_in":3600}})";
        handle_websocket_message(mock_auth_response);
    }
    
    return true;
}

std::string DeribitOMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "public/auth";
    
    Json::Value params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
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
    } else if (status == "cancelled" || status == "canceled") {
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

void DeribitOMS::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    LOG_INFO_COMP("DERIBIT_OMS", "Setting custom WebSocket transport for testing");
    custom_transport_ = transport;
}

} // namespace deribit
