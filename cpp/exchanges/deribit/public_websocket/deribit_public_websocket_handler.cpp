#include "deribit_public_websocket_handler.hpp"
#include <iostream>
#include <sstream>
#include <json/json.h>

namespace deribit {

DeribitPublicWebSocketHandler::DeribitPublicWebSocketHandler() {
    std::cout << "[DERIBIT_PUBLIC_WS] Handler created" << std::endl;
}

DeribitPublicWebSocketHandler::~DeribitPublicWebSocketHandler() {
    shutdown();
}

bool DeribitPublicWebSocketHandler::connect(const std::string& url) {
    std::cout << "[DERIBIT_PUBLIC_WS] Connecting to: " << url << std::endl;
    
    // Mock connection for testing
    connected_ = true;
    state_ = WebSocketState::CONNECTED;
    
    if (connect_callback_) {
        connect_callback_(true);
    }
    
    return true;
}

void DeribitPublicWebSocketHandler::disconnect() {
    std::cout << "[DERIBIT_PUBLIC_WS] Disconnecting" << std::endl;
    connected_ = false;
    state_ = WebSocketState::DISCONNECTED;
}

bool DeribitPublicWebSocketHandler::is_connected() const {
    return connected_;
}

WebSocketState DeribitPublicWebSocketHandler::get_state() const {
    return state_;
}

bool DeribitPublicWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!connected_) {
        std::cerr << "[DERIBIT_PUBLIC_WS] Cannot send message - not connected" << std::endl;
        return false;
    }
    
    std::cout << "[DERIBIT_PUBLIC_WS] Sending message: " << message.substr(0, 100) << std::endl;
    
    // Mock message handling
    handle_message(message);
    return true;
}

bool DeribitPublicWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    std::cerr << "[DERIBIT_PUBLIC_WS] Binary messages not supported" << std::endl;
    return false;
}

void DeribitPublicWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void DeribitPublicWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void DeribitPublicWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void DeribitPublicWebSocketHandler::set_ping_interval(int seconds) {
    std::cout << "[DERIBIT_PUBLIC_WS] Ping interval set to: " << seconds << " seconds" << std::endl;
}

void DeribitPublicWebSocketHandler::set_timeout(int seconds) {
    std::cout << "[DERIBIT_PUBLIC_WS] Timeout set to: " << seconds << " seconds" << std::endl;
}

void DeribitPublicWebSocketHandler::set_reconnect_attempts(int attempts) {
    std::cout << "[DERIBIT_PUBLIC_WS] Reconnect attempts set to: " << attempts << std::endl;
}

void DeribitPublicWebSocketHandler::set_reconnect_delay(int seconds) {
    std::cout << "[DERIBIT_PUBLIC_WS] Reconnect delay set to: " << seconds << " seconds" << std::endl;
}

bool DeribitPublicWebSocketHandler::initialize() {
    std::cout << "[DERIBIT_PUBLIC_WS] Initializing" << std::endl;
    return true;
}

void DeribitPublicWebSocketHandler::shutdown() {
    std::cout << "[DERIBIT_PUBLIC_WS] Shutting down" << std::endl;
    should_stop_ = true;
    disconnect();
}

bool DeribitPublicWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    // Check if already subscribed
    for (const auto& ch : subscribed_channels_) {
        if (ch == channel) {
            std::cout << "[DERIBIT_PUBLIC_WS] Already subscribed to: " << channel << std::endl;
            return true;
        }
    }
    
    subscribed_channels_.push_back(channel);
    std::cout << "[DERIBIT_PUBLIC_WS] Subscribed to channel: " << channel << std::endl;
    
    // Build and send subscription message
    std::vector<std::string> channels = {channel};
    std::string sub_msg = build_subscription_message("public/subscribe", channels);
    return send_message(sub_msg);
}

bool DeribitPublicWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[DERIBIT_PUBLIC_WS] Unsubscribed from channel: " << channel << std::endl;
        
        // Build and send unsubscription message
        std::vector<std::string> channels = {channel};
        std::string unsub_msg = build_subscription_message("public/unsubscribe", channels);
        return send_message(unsub_msg);
    }
    
    return false;
}

std::vector<std::string> DeribitPublicWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void DeribitPublicWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    std::cout << "[DERIBIT_PUBLIC_WS] Auth credentials set (not used for public streams)" << std::endl;
}

bool DeribitPublicWebSocketHandler::subscribe_to_orderbook(const std::string& symbol, const std::string& interval) {
    std::string channel = "book." + symbol + "." + interval;
    return subscribe_to_channel(channel);
}

bool DeribitPublicWebSocketHandler::subscribe_to_trades(const std::string& symbol) {
    std::string channel = "trades." + symbol + ".raw";
    return subscribe_to_channel(channel);
}

bool DeribitPublicWebSocketHandler::subscribe_to_ticker(const std::string& symbol) {
    std::string channel = "ticker." + symbol + ".raw";
    return subscribe_to_channel(channel);
}

bool DeribitPublicWebSocketHandler::subscribe_to_instruments(const std::string& currency) {
    std::string channel = "instruments." + currency;
    return subscribe_to_channel(channel);
}

void DeribitPublicWebSocketHandler::handle_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[DERIBIT_PUBLIC_WS] Failed to parse JSON: " << message.substr(0, 100) << std::endl;
            return;
        }
        
        // Check if it's a subscription response
        if (root.isMember("id") && root.isMember("result")) {
            std::cout << "[DERIBIT_PUBLIC_WS] Subscription response received" << std::endl;
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
                    
                    if (channel_str.find("book.") == 0) {
                        process_orderbook_update(data.toStyledString());
                    } else if (channel_str.find("trades.") == 0) {
                        process_trade_update(data.toStyledString());
                    } else if (channel_str.find("ticker.") == 0) {
                        process_ticker_update(data.toStyledString());
                    } else if (channel_str.find("instruments.") == 0) {
                        process_instrument_update(data.toStyledString());
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
            ws_msg.channel = "deribit_market_data";
            message_callback_(ws_msg);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_PUBLIC_WS] Error processing message: " << e.what() << std::endl;
    }
}

std::string DeribitPublicWebSocketHandler::build_subscription_message(const std::string& method, const std::vector<std::string>& channels) {
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

void DeribitPublicWebSocketHandler::process_orderbook_update(const std::string& message) {
    std::cout << "[DERIBIT_PUBLIC_WS] Orderbook update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_orderbook";
        message_callback_(ws_msg);
    }
}

void DeribitPublicWebSocketHandler::process_trade_update(const std::string& message) {
    std::cout << "[DERIBIT_PUBLIC_WS] Trade update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_trades";
        message_callback_(ws_msg);
    }
}

void DeribitPublicWebSocketHandler::process_ticker_update(const std::string& message) {
    std::cout << "[DERIBIT_PUBLIC_WS] Ticker update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_ticker";
        message_callback_(ws_msg);
    }
}

void DeribitPublicWebSocketHandler::process_instrument_update(const std::string& message) {
    std::cout << "[DERIBIT_PUBLIC_WS] Instrument update: " << message.substr(0, 100) << std::endl;
    
    if (message_callback_) {
        WebSocketMessage ws_msg;
        ws_msg.data = message;
        ws_msg.is_binary = false;
        ws_msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_msg.channel = "deribit_instruments";
        message_callback_(ws_msg);
    }
}

} // namespace deribit
