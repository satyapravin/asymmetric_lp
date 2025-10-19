#include "deribit_private_websocket_handler.hpp"
#include <iostream>
#include <sstream>
#include <json/json.h>
#include <chrono>

namespace deribit {

DeribitPrivateWebSocketHandler::DeribitPrivateWebSocketHandler(const std::string& client_id, const std::string& client_secret)
    : client_id_(client_id), client_secret_(client_secret) {
    std::cout << "[DERIBIT_PRIVATE_WS] Handler created with client_id: " << client_id_ << std::endl;
}

DeribitPrivateWebSocketHandler::~DeribitPrivateWebSocketHandler() {
    shutdown();
}

bool DeribitPrivateWebSocketHandler::connect(const std::string& url) {
    std::cout << "[DERIBIT_PRIVATE_WS] Connecting to: " << url << std::endl;
    
    // Mock connection for testing
    connected_ = true;
    state_ = WebSocketState::CONNECTED;
    
    if (connect_callback_) {
        connect_callback_(true);
    }
    
    // Authenticate after connection
    return authenticate();
}

void DeribitPrivateWebSocketHandler::disconnect() {
    std::cout << "[DERIBIT_PRIVATE_WS] Disconnecting" << std::endl;
    connected_ = false;
    authenticated_ = false;
    state_ = WebSocketState::DISCONNECTED;
}

bool DeribitPrivateWebSocketHandler::is_connected() const {
    return connected_;
}

WebSocketState DeribitPrivateWebSocketHandler::get_state() const {
    return state_;
}

bool DeribitPrivateWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!connected_) {
        std::cerr << "[DERIBIT_PRIVATE_WS] Cannot send message - not connected" << std::endl;
        return false;
    }
    
    std::cout << "[DERIBIT_PRIVATE_WS] Sending message: " << message.substr(0, 100) << std::endl;
    
    // Mock message handling
    handle_message(message);
    return true;
}

bool DeribitPrivateWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    std::cerr << "[DERIBIT_PRIVATE_WS] Binary messages not supported" << std::endl;
    return false;
}

void DeribitPrivateWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void DeribitPrivateWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void DeribitPrivateWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void DeribitPrivateWebSocketHandler::set_ping_interval(int seconds) {
    std::cout << "[DERIBIT_PRIVATE_WS] Ping interval set to: " << seconds << " seconds" << std::endl;
}

void DeribitPrivateWebSocketHandler::set_timeout(int seconds) {
    std::cout << "[DERIBIT_PRIVATE_WS] Timeout set to: " << seconds << " seconds" << std::endl;
}

void DeribitPrivateWebSocketHandler::set_reconnect_attempts(int attempts) {
    std::cout << "[DERIBIT_PRIVATE_WS] Reconnect attempts set to: " << attempts << std::endl;
}

void DeribitPrivateWebSocketHandler::set_reconnect_delay(int seconds) {
    std::cout << "[DERIBIT_PRIVATE_WS] Reconnect delay set to: " << seconds << " seconds" << std::endl;
}

bool DeribitPrivateWebSocketHandler::initialize() {
    std::cout << "[DERIBIT_PRIVATE_WS] Initializing" << std::endl;
    return true;
}

void DeribitPrivateWebSocketHandler::shutdown() {
    std::cout << "[DERIBIT_PRIVATE_WS] Shutting down" << std::endl;
    should_stop_ = true;
    refresh_thread_running_ = false;
    
    if (token_refresh_thread_.joinable()) {
        token_refresh_thread_.join();
    }
    
    disconnect();
}

bool DeribitPrivateWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    if (!authenticated_) {
        std::cerr << "[DERIBIT_PRIVATE_WS] Cannot subscribe - not authenticated" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    // Check if already subscribed
    for (const auto& ch : subscribed_channels_) {
        if (ch == channel) {
            std::cout << "[DERIBIT_PRIVATE_WS] Already subscribed to: " << channel << std::endl;
            return true;
        }
    }
    
    subscribed_channels_.push_back(channel);
    std::cout << "[DERIBIT_PRIVATE_WS] Subscribed to channel: " << channel << std::endl;
    
    // Build and send subscription message
    std::vector<std::string> channels = {channel};
    std::string sub_msg = build_subscription_message("private/subscribe", channels);
    return send_message(sub_msg);
}

bool DeribitPrivateWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[DERIBIT_PRIVATE_WS] Unsubscribed from channel: " << channel << std::endl;
        
        // Build and send unsubscription message
        std::vector<std::string> channels = {channel};
        std::string unsub_msg = build_subscription_message("private/unsubscribe", channels);
        return send_message(unsub_msg);
    }
    
    return false;
}

std::vector<std::string> DeribitPrivateWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void DeribitPrivateWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    client_id_ = api_key;
    client_secret_ = secret;
    std::cout << "[DERIBIT_PRIVATE_WS] Auth credentials updated" << std::endl;
}

bool DeribitPrivateWebSocketHandler::is_authenticated() const {
    return authenticated_;
}

bool DeribitPrivateWebSocketHandler::subscribe_to_user_data() {
    return subscribe_to_channel("user.portfolio.btc");
}

bool DeribitPrivateWebSocketHandler::subscribe_to_order_updates(const std::string& symbol) {
    std::string channel = symbol.empty() ? "user.orders.any.raw" : "user.orders." + symbol + ".raw";
    return subscribe_to_channel(channel);
}

bool DeribitPrivateWebSocketHandler::subscribe_to_account_updates() {
    return subscribe_to_channel("user.changes.any.any");
}

bool DeribitPrivateWebSocketHandler::subscribe_to_portfolio_updates() {
    return subscribe_to_channel("user.portfolio.btc");
}

bool DeribitPrivateWebSocketHandler::subscribe_to_position_updates() {
    return subscribe_to_channel("user.portfolio.btc");
}

bool DeribitPrivateWebSocketHandler::authenticate() {
    std::cout << "[DERIBIT_PRIVATE_WS] Authenticating with client_id: " << client_id_ << std::endl;
    
    // Mock authentication for testing
    access_token_ = "mock_access_token_" + client_id_;
    authenticated_ = true;
    
    std::cout << "[DERIBIT_PRIVATE_WS] Authentication successful" << std::endl;
    
    // Start token refresh thread
    if (!refresh_thread_running_) {
        refresh_thread_running_ = true;
        token_refresh_thread_ = std::thread(&DeribitPrivateWebSocketHandler::token_refresh_loop, this);
    }
    
    return true;
}

bool DeribitPrivateWebSocketHandler::refresh_token() {
    std::cout << "[DERIBIT_PRIVATE_WS] Refreshing access token" << std::endl;
    
    // Mock token refresh for testing
    access_token_ = "refreshed_access_token_" + client_id_;
    
    std::cout << "[DERIBIT_PRIVATE_WS] Token refresh successful" << std::endl;
    return true;
}

void DeribitPrivateWebSocketHandler::handle_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[DERIBIT_PRIVATE_WS] Failed to parse JSON: " << message.substr(0, 100) << std::endl;
            return;
        }
        
        // Check if it's a subscription response
        if (root.isMember("id") && root.isMember("result")) {
            std::cout << "[DERIBIT_PRIVATE_WS] Subscription response received" << std::endl;
            return;
        }
        
        // Check if it's a notification
        if (root.isMember("method") && root.isMember("params")) {
            std::string method = root["method"].asString();
            Json::Value params = root["params"];
            
            if (method == "subscription") {
                Json::Value channel = params["channel"];
                Json::Value data = params["data"];
                
                if (channel.isString()) {
                    std::string channel_str = channel.asString();
                    
                    if (channel_str.find("user.orders.") == 0) {
                        process_order_update(data.toStyledString());
                    } else if (channel_str.find("user.changes.") == 0) {
                        process_account_update(data.toStyledString());
                    } else if (channel_str.find("user.portfolio.") == 0) {
                        process_portfolio_update(data.toStyledString());
                    }
                }
            }
        }
        
        // Forward to callback
        if (message_callback_) {
            WebSocketMessage ws_msg;
            ws_msg.data = message;
            ws_msg.is_binary = false;
            ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            ws_msg.channel = "deribit_private";
            message_callback_(ws_msg);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_PRIVATE_WS] Error processing message: " << e.what() << std::endl;
    }
}

void DeribitPrivateWebSocketHandler::token_refresh_loop() {
    std::cout << "[DERIBIT_PRIVATE_WS] Token refresh thread started" << std::endl;
    
    while (refresh_thread_running_ && !should_stop_) {
        std::this_thread::sleep_for(std::chrono::minutes(30)); // Refresh every 30 minutes
        
        if (refresh_thread_running_ && !should_stop_) {
            refresh_token();
        }
    }
    
    std::cout << "[DERIBIT_PRIVATE_WS] Token refresh thread ended" << std::endl;
}

std::string DeribitPrivateWebSocketHandler::build_auth_message() {
    Json::Value root;
    root["jsonrpc"] = api_version_;
    root["id"] = request_id_++;
    root["method"] = "public/auth";
    
    Json::Value params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = client_id_;
    params["client_secret"] = client_secret_;
    root["params"] = params;
    
    return root.toStyledString();
}

std::string DeribitPrivateWebSocketHandler::build_subscription_message(const std::string& method, const std::vector<std::string>& channels) {
    Json::Value root;
    root["jsonrpc"] = api_version_;
    root["id"] = request_id_++;
    root["method"] = method;
    
    Json::Value params(Json::arrayValue);
    for (const auto& channel : channels) {
        params.append(channel);
    }
    root["params"] = params;
    
    return root.toStyledString();
}

void DeribitPrivateWebSocketHandler::process_order_update(const std::string& message) {
    std::cout << "[DERIBIT_PRIVATE_WS] Order update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_orders";
        message_callback_(ws_msg);
    }
}

void DeribitPrivateWebSocketHandler::process_account_update(const std::string& message) {
    std::cout << "[DERIBIT_PRIVATE_WS] Account update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_account";
        message_callback_(ws_msg);
    }
}

void DeribitPrivateWebSocketHandler::process_portfolio_update(const std::string& message) {
    std::cout << "[DERIBIT_PRIVATE_WS] Portfolio update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_portfolio";
        message_callback_(ws_msg);
    }
}

void DeribitPrivateWebSocketHandler::process_position_update(const std::string& message) {
    std::cout << "[DERIBIT_PRIVATE_WS] Position update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_positions";
        message_callback_(ws_msg);
    }
}

} // namespace deribit
