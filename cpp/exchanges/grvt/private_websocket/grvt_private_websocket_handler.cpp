#include "grvt_private_websocket_handler.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <json/json.h>

namespace grvt {

GrvtPrivateWebSocketHandler::GrvtPrivateWebSocketHandler(const std::string& api_key, const std::string& session_cookie, const std::string& account_id)
    : api_key_(api_key), session_cookie_(session_cookie), account_id_(account_id) {
    std::cout << "[GRVT_PRIVATE_WS] Initializing private WebSocket handler" << std::endl;
    base_url_ = "wss://trades.grvt.io/ws/full"; // Default to full version
}

GrvtPrivateWebSocketHandler::~GrvtPrivateWebSocketHandler() {
    shutdown();
}

bool GrvtPrivateWebSocketHandler::connect(const std::string& url) {
    std::cout << "[GRVT_PRIVATE_WS] Connecting to: " << url << std::endl;
    
    if (api_key_.empty() || session_cookie_.empty() || account_id_.empty()) {
        std::cerr << "[GRVT_PRIVATE_WS] API Key, Session Cookie, and Account ID are required for private WebSocket connections." << std::endl;
        if (error_callback_) {
            error_callback_("Authentication failed: Missing credentials.");
        }
        return false;
    }

    // Simulate connection and authentication
    state_ = WebSocketState::CONNECTING;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate connection delay

    // For mock, assume connection is successful if credentials are provided
    if (!api_key_.empty() && !session_cookie_.empty() && !account_id_.empty()) {
        connected_ = true;
        state_ = WebSocketState::CONNECTED;
        if (connect_callback_) {
            connect_callback_(true);
        }
        
        // Start session refresh loop
        if (!refresh_thread_running_.load()) {
            refresh_thread_running_ = true;
            session_refresh_thread_ = std::thread(&GrvtPrivateWebSocketHandler::session_refresh_loop, this);
        }
        
        return true;
    } else {
        connected_ = false;
        state_ = WebSocketState::DISCONNECTED;
        if (connect_callback_) {
            connect_callback_(false);
        }
        return false;
    }
}

void GrvtPrivateWebSocketHandler::disconnect() {
    std::cout << "[GRVT_PRIVATE_WS] Disconnecting" << std::endl;
    state_ = WebSocketState::DISCONNECTING;
    connected_ = false;
    state_ = WebSocketState::DISCONNECTED;
    
    if (connect_callback_) {
        connect_callback_(false);
    }
    
    refresh_thread_running_ = false;
    if (session_refresh_thread_.joinable()) {
        session_refresh_thread_.join();
    }
}

bool GrvtPrivateWebSocketHandler::is_connected() const {
    return connected_.load();
}

WebSocketState GrvtPrivateWebSocketHandler::get_state() const {
    return state_.load();
}

bool GrvtPrivateWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[GRVT_PRIVATE_WS] Sending message: " << message << std::endl;
    // Simulate sending message
    return true;
}

bool GrvtPrivateWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[GRVT_PRIVATE_WS] Sending binary data: " << data.size() << " bytes" << std::endl;
    // Simulate sending binary data
    return true;
}

void GrvtPrivateWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void GrvtPrivateWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void GrvtPrivateWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void GrvtPrivateWebSocketHandler::set_ping_interval(int seconds) {
    // Not implemented for mock
}

void GrvtPrivateWebSocketHandler::set_timeout(int seconds) {
    // Not implemented for mock
}

void GrvtPrivateWebSocketHandler::set_reconnect_attempts(int attempts) {
    // Not implemented for mock
}

void GrvtPrivateWebSocketHandler::set_reconnect_delay(int seconds) {
    // Not implemented for mock
}

bool GrvtPrivateWebSocketHandler::initialize() {
    std::cout << "[GRVT_PRIVATE_WS] Initializing" << std::endl;
    return true;
}

void GrvtPrivateWebSocketHandler::shutdown() {
    std::cout << "[GRVT_PRIVATE_WS] Shutting down" << std::endl;
    should_stop_ = true;
    disconnect();
}

bool GrvtPrivateWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    subscribed_channels_.push_back(channel);
    std::cout << "[GRVT_PRIVATE_WS] Subscribed to channel: " << channel << std::endl;
    return true;
}

bool GrvtPrivateWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[GRVT_PRIVATE_WS] Unsubscribed from channel: " << channel << std::endl;
        return true;
    }
    return false;
}

std::vector<std::string> GrvtPrivateWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void GrvtPrivateWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    api_key_ = api_key;
    // Note: GRVT uses session cookies instead of secrets
}

std::string GrvtPrivateWebSocketHandler::get_channel() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    if (subscribed_channels_.empty()) {
        return "";
    }
    return subscribed_channels_[0]; // Return first subscribed channel
}

bool GrvtPrivateWebSocketHandler::is_authenticated() const {
    return !api_key_.empty() && !session_cookie_.empty() && !account_id_.empty();
}

bool GrvtPrivateWebSocketHandler::subscribe_to_user_data() {
    return subscribe_to_channel("userDataStream");
}

bool GrvtPrivateWebSocketHandler::subscribe_to_order_updates() {
    return subscribe_to_channel("orderUpdates");
}

bool GrvtPrivateWebSocketHandler::subscribe_to_account_updates() {
    return subscribe_to_channel("accountUpdates");
}

bool GrvtPrivateWebSocketHandler::subscribe_to_balance_updates() {
    return subscribe_to_channel("balanceUpdates");
}

bool GrvtPrivateWebSocketHandler::subscribe_to_position_updates() {
    return subscribe_to_channel("positionUpdates");
}

bool GrvtPrivateWebSocketHandler::authenticate() {
    if (api_key_.empty() || session_cookie_.empty() || account_id_.empty()) {
        std::cerr << "[GRVT_PRIVATE_WS] Cannot authenticate: Missing credentials." << std::endl;
        return false;
    }
    
    // Simulate authentication
    std::cout << "[GRVT_PRIVATE_WS] Authenticated with API key: " << api_key_ << std::endl;
    std::cout << "[GRVT_PRIVATE_WS] Session cookie: " << session_cookie_ << std::endl;
    std::cout << "[GRVT_PRIVATE_WS] Account ID: " << account_id_ << std::endl;
    
    return true;
}

bool GrvtPrivateWebSocketHandler::refresh_session() {
    if (session_cookie_.empty()) {
        std::cerr << "[GRVT_PRIVATE_WS] Cannot refresh session: No session cookie available." << std::endl;
        return false;
    }
    
    // Simulate session refresh
    std::cout << "[GRVT_PRIVATE_WS] Refreshed session cookie: " << session_cookie_ << std::endl;
    return true;
}

bool GrvtPrivateWebSocketHandler::validate_session() {
    if (session_cookie_.empty() || account_id_.empty()) {
        std::cerr << "[GRVT_PRIVATE_WS] Cannot validate session: Missing session data." << std::endl;
        return false;
    }
    
    // Simulate session validation
    std::cout << "[GRVT_PRIVATE_WS] Session validated successfully" << std::endl;
    return true;
}

bool GrvtPrivateWebSocketHandler::send_jsonrpc_request(const std::string& method, const std::string& params, int request_id) {
    if (!connected_.load()) {
        return false;
    }
    
    std::string request = create_jsonrpc_request(method, params, request_id);
    return send_message(request);
}

bool GrvtPrivateWebSocketHandler::send_lite_jsonrpc_request(const std::string& method, const std::string& params, int request_id) {
    if (!connected_.load()) {
        return false;
    }
    
    std::string request = create_lite_jsonrpc_request(method, params, request_id);
    return send_message(request);
}

void GrvtPrivateWebSocketHandler::handle_message(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        message_callback_(ws_message);
    }
    
    // Parse JSON-RPC response
    parse_jsonrpc_response(message);
}

void GrvtPrivateWebSocketHandler::handle_order_update(const std::string& order_id, const std::string& status) {
    std::cout << "[GRVT_PRIVATE_WS] Order update: " << order_id << " -> " << status << std::endl;
    
    if (order_callback_) {
        order_callback_(order_id, status);
    }
}

void GrvtPrivateWebSocketHandler::handle_account_update(const std::string& account_id, const std::string& data) {
    std::cout << "[GRVT_PRIVATE_WS] Account update: " << account_id << " -> " << data << std::endl;
    
    if (account_callback_) {
        account_callback_(account_id, data);
    }
}

void GrvtPrivateWebSocketHandler::session_refresh_loop() {
    while (refresh_thread_running_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(30)); // Refresh every 30 minutes
        if (refresh_thread_running_.load()) {
            refresh_session();
        }
    }
}

bool GrvtPrivateWebSocketHandler::parse_jsonrpc_response(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[GRVT_PRIVATE_WS] Failed to parse JSON-RPC response: " << message << std::endl;
            return false;
        }
        
        // Check if it's a JSON-RPC response
        if (root.isMember("result") || root.isMember("r")) {
            // Handle successful response
            std::cout << "[GRVT_PRIVATE_WS] Received JSON-RPC response" << std::endl;
        } else if (root.isMember("error") || root.isMember("e")) {
            // Handle error response
            std::string error_msg = root.isMember("error") ? root["error"].asString() : root["e"].asString();
            std::cerr << "[GRVT_PRIVATE_WS] JSON-RPC error: " << error_msg << std::endl;
            
            if (error_callback_) {
                error_callback_(error_msg);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_PRIVATE_WS] Error parsing JSON-RPC response: " << e.what() << std::endl;
        return false;
    }
}

std::string GrvtPrivateWebSocketHandler::create_jsonrpc_request(const std::string& method, const std::string& params, int request_id) {
    Json::Value request;
    request["jsonrpc"] = "2.0";
    request["method"] = method;
    request["params"] = params;
    request["id"] = request_id;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, request);
}

std::string GrvtPrivateWebSocketHandler::create_lite_jsonrpc_request(const std::string& method, const std::string& params, int request_id) {
    Json::Value request;
    request["j"] = "2.0";
    request["m"] = method;
    request["p"] = params;
    request["i"] = request_id;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, request);
}

} // namespace grvt
