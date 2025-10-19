#include "grvt_public_websocket_handler.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <json/json.h>

namespace grvt {

GrvtPublicWebSocketHandler::GrvtPublicWebSocketHandler() {
    std::cout << "[GRVT_PUBLIC_WS] Initializing public WebSocket handler" << std::endl;
    base_url_ = "wss://trades.grvt.io/ws/full"; // Default to full version
}

GrvtPublicWebSocketHandler::~GrvtPublicWebSocketHandler() {
    shutdown();
}

bool GrvtPublicWebSocketHandler::connect(const std::string& url) {
    std::cout << "[GRVT_PUBLIC_WS] Connecting to: " << url << std::endl;
    state_ = WebSocketState::CONNECTING;
    
    // For public streams, no authentication is required
    base_url_ = url;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate connection delay
    connected_ = true;
    state_ = WebSocketState::CONNECTED;
    
    if (connect_callback_) {
        connect_callback_(true);
    }
    
    return true;
}

void GrvtPublicWebSocketHandler::disconnect() {
    std::cout << "[GRVT_PUBLIC_WS] Disconnecting" << std::endl;
    state_ = WebSocketState::DISCONNECTING;
    connected_ = false;
    state_ = WebSocketState::DISCONNECTED;
    
    if (connect_callback_) {
        connect_callback_(false);
    }
}

bool GrvtPublicWebSocketHandler::is_connected() const {
    return connected_.load();
}

WebSocketState GrvtPublicWebSocketHandler::get_state() const {
    return state_.load();
}

bool GrvtPublicWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[GRVT_PUBLIC_WS] Sending message: " << message << std::endl;
    // Simulate sending message
    return true;
}

bool GrvtPublicWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "[GRVT_PUBLIC_WS] Sending binary data: " << data.size() << " bytes" << std::endl;
    // Simulate sending binary data
    return true;
}

void GrvtPublicWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
}

void GrvtPublicWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
}

void GrvtPublicWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
}

void GrvtPublicWebSocketHandler::set_ping_interval(int seconds) {
    // Not implemented for mock
}

void GrvtPublicWebSocketHandler::set_timeout(int seconds) {
    // Not implemented for mock
}

void GrvtPublicWebSocketHandler::set_reconnect_attempts(int attempts) {
    // Not implemented for mock
}

void GrvtPublicWebSocketHandler::set_reconnect_delay(int seconds) {
    // Not implemented for mock
}

bool GrvtPublicWebSocketHandler::initialize() {
    std::cout << "[GRVT_PUBLIC_WS] Initializing" << std::endl;
    return true;
}

void GrvtPublicWebSocketHandler::shutdown() {
    std::cout << "[GRVT_PUBLIC_WS] Shutting down" << std::endl;
    should_stop_ = true;
    disconnect();
}

bool GrvtPublicWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    subscribed_channels_.push_back(channel);
    std::cout << "[GRVT_PUBLIC_WS] Subscribed to channel: " << channel << std::endl;
    return true;
}

bool GrvtPublicWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[GRVT_PUBLIC_WS] Unsubscribed from channel: " << channel << std::endl;
        return true;
    }
    return false;
}

std::vector<std::string> GrvtPublicWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void GrvtPublicWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    // Public streams don't need authentication
}

std::string GrvtPublicWebSocketHandler::get_channel() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    if (subscribed_channels_.empty()) {
        return "";
    }
    return subscribed_channels_[0]; // Return first subscribed channel
}

bool GrvtPublicWebSocketHandler::is_authenticated() const {
    // Public streams don't require authentication
    return true;
}

bool GrvtPublicWebSocketHandler::subscribe_to_orderbook(const std::string& symbol) {
    std::stringstream stream;
    stream << "orderbook." << symbol;
    return subscribe_to_channel(stream.str());
}

bool GrvtPublicWebSocketHandler::subscribe_to_trades(const std::string& symbol) {
    std::stringstream stream;
    stream << "trades." << symbol;
    return subscribe_to_channel(stream.str());
}

bool GrvtPublicWebSocketHandler::subscribe_to_ticker(const std::string& symbol) {
    std::stringstream stream;
    stream << "ticker." << symbol;
    return subscribe_to_channel(stream.str());
}

bool GrvtPublicWebSocketHandler::subscribe_to_market_data(const std::string& symbol) {
    std::stringstream stream;
    stream << "market_data." << symbol;
    return subscribe_to_channel(stream.str());
}

void GrvtPublicWebSocketHandler::handle_message(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        message_callback_(ws_message);
    }
    
    // Parse and handle specific message types
    parse_json_message(message);
}

void GrvtPublicWebSocketHandler::handle_orderbook_update(const std::string& symbol, const std::string& data) {
    std::cout << "[GRVT_PUBLIC_WS] Orderbook update for " << symbol << ": " << data << std::endl;
    process_orderbook_data(symbol, data);
}

void GrvtPublicWebSocketHandler::handle_trade_update(const std::string& symbol, const std::string& data) {
    std::cout << "[GRVT_PUBLIC_WS] Trade update for " << symbol << ": " << data << std::endl;
    process_trade_data(symbol, data);
}

void GrvtPublicWebSocketHandler::handle_ticker_update(const std::string& symbol, const std::string& data) {
    std::cout << "[GRVT_PUBLIC_WS] Ticker update for " << symbol << ": " << data << std::endl;
    process_ticker_data(symbol, data);
}

bool GrvtPublicWebSocketHandler::parse_json_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[GRVT_PUBLIC_WS] Failed to parse JSON: " << message << std::endl;
            return false;
        }
        
        // Check if it's a GRVT message format
        if (root.isMember("method") || root.isMember("m")) {
            std::string method = root.isMember("method") ? root["method"].asString() : root["m"].asString();
            
            if (method.find("orderbook") != std::string::npos) {
                // Handle orderbook update
                std::string symbol = "BTCUSDT"; // Extract from params
                handle_orderbook_update(symbol, message);
            } else if (method.find("trade") != std::string::npos) {
                // Handle trade update
                std::string symbol = "BTCUSDT"; // Extract from params
                handle_trade_update(symbol, message);
            } else if (method.find("ticker") != std::string::npos) {
                // Handle ticker update
                std::string symbol = "BTCUSDT"; // Extract from params
                handle_ticker_update(symbol, message);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_PUBLIC_WS] Error parsing message: " << e.what() << std::endl;
        return false;
    }
}

void GrvtPublicWebSocketHandler::process_orderbook_data(const std::string& symbol, const std::string& data) {
    // Mock orderbook data processing
    std::vector<std::pair<double, double>> bids = {{50000.0, 1.5}, {49999.0, 2.0}};
    std::vector<std::pair<double, double>> asks = {{50001.0, 1.0}, {50002.0, 1.5}};
    
    if (orderbook_callback_) {
        orderbook_callback_(symbol, bids, asks);
    }
}

void GrvtPublicWebSocketHandler::process_trade_data(const std::string& symbol, const std::string& data) {
    // Mock trade data processing
    double price = 50000.0;
    double qty = 0.1;
    
    if (trade_callback_) {
        trade_callback_(symbol, price, qty);
    }
}

void GrvtPublicWebSocketHandler::process_ticker_data(const std::string& symbol, const std::string& data) {
    // Mock ticker data processing
    double price = 50000.0;
    double volume = 100.5;
    
    if (ticker_callback_) {
        ticker_callback_(symbol, price, volume);
    }
}

} // namespace grvt
