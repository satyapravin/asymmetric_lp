#include "binance_public_websocket_handler.hpp"
#include "../../websocket/websocket_transport.hpp"
#include "../../../proto/market_data.pb.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <json/json.h>

namespace binance {

BinancePublicWebSocketHandler::BinancePublicWebSocketHandler() {
    std::cout << "[BINANCE] Initializing Binance Public WebSocket Handler with transport abstraction" << std::endl;
    
    // Create transport using factory
    transport_ = websocket_transport::WebSocketTransportFactory::create();
    
    // Set up transport callbacks
    transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& message) {
        handle_websocket_message(message);
    });
    
    transport_->set_error_callback([this](int error_code, const std::string& error_message) {
        handle_connection_error(error_code, error_message);
    });
    
    transport_->set_connect_callback([this](bool connected) {
        handle_connection_status(connected);
    });
    
    std::cout << "[BINANCE] Transport abstraction initialization complete" << std::endl;
}

BinancePublicWebSocketHandler::BinancePublicWebSocketHandler(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    std::cout << "[BINANCE] Initializing Binance Public WebSocket Handler with injected transport" << std::endl;
    
    // Use injected transport
    transport_ = std::move(transport);
    
    // Set up transport callbacks
    transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& message) {
        handle_websocket_message(message);
    });
    
    transport_->set_error_callback([this](int error_code, const std::string& error_message) {
        handle_connection_error(error_code, error_message);
    });
    
    transport_->set_connect_callback([this](bool connected) {
        handle_connection_status(connected);
    });
    
    std::cout << "[BINANCE] Injected transport initialization complete" << std::endl;
}

BinancePublicWebSocketHandler::~BinancePublicWebSocketHandler() {
    std::cout << "[BINANCE] Destroying Binance Public WebSocket Handler" << std::endl;
    disconnect();
    shutdown();
}

bool BinancePublicWebSocketHandler::connect(const std::string& url) {
    std::cout << "[BINANCE] Connecting to public WebSocket: " << url << std::endl;
    
    if (!transport_) {
        std::cerr << "[BINANCE] Transport not initialized" << std::endl;
        return false;
    }
    
    return transport_->connect(url);
}

void BinancePublicWebSocketHandler::disconnect() {
    std::cout << "[BINANCE] Disconnecting from public WebSocket" << std::endl;
    
    if (transport_) {
        transport_->disconnect();
    }
}

bool BinancePublicWebSocketHandler::is_connected() const {
    return transport_ ? transport_->is_connected() : false;
}

WebSocketState BinancePublicWebSocketHandler::get_state() const {
    return transport_ ? static_cast<WebSocketState>(transport_->get_state()) : WebSocketState::DISCONNECTED;
}

bool BinancePublicWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!transport_) {
        std::cerr << "[BINANCE] Transport not initialized" << std::endl;
        return false;
    }
    
    return transport_->send_message(message, binary);
}

bool BinancePublicWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!transport_) {
        std::cerr << "[BINANCE] Transport not initialized" << std::endl;
        return false;
    }
    
    return transport_->send_binary(data);
}

void BinancePublicWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
    std::cout << "[BINANCE] Message callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
    std::cout << "[BINANCE] Error callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
    std::cout << "[BINANCE] Connect callback set" << std::endl;
}

void BinancePublicWebSocketHandler::set_ping_interval(int seconds) {
    if (transport_) {
        transport_->set_ping_interval(seconds);
    }
}

void BinancePublicWebSocketHandler::set_timeout(int seconds) {
    if (transport_) {
        transport_->set_timeout(seconds);
    }
}

void BinancePublicWebSocketHandler::set_reconnect_attempts(int attempts) {
    if (transport_) {
        transport_->set_reconnect_attempts(attempts);
    }
}

void BinancePublicWebSocketHandler::set_reconnect_delay(int seconds) {
    if (transport_) {
        transport_->set_reconnect_delay(seconds);
    }
}

void BinancePublicWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    // Public streams don't require authentication
    std::cout << "[BINANCE] Public WebSocket doesn't require authentication" << std::endl;
}

bool BinancePublicWebSocketHandler::initialize() {
    std::cout << "[BINANCE] Initializing public WebSocket handler" << std::endl;
    
    if (!transport_) {
        std::cerr << "[BINANCE] Transport not initialized" << std::endl;
        return false;
    }
    
    return transport_->initialize();
}

void BinancePublicWebSocketHandler::shutdown() {
    std::cout << "[BINANCE] Shutting down public WebSocket handler" << std::endl;
    
    if (transport_) {
        transport_->shutdown();
    }
}

bool BinancePublicWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot subscribe: not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    subscribed_channels_.push_back(channel);
    
    // Send subscription message
    Json::Value subscribe_msg;
    subscribe_msg["method"] = "SUBSCRIBE";
    subscribe_msg["params"] = Json::Value(Json::arrayValue);
    subscribe_msg["params"].append(channel);
    subscribe_msg["id"] = 1;
    
    Json::StreamWriterBuilder builder;
    std::string message = Json::writeString(builder, subscribe_msg);
    
    if (send_message(message)) {
    std::cout << "[BINANCE] Subscribed to channel: " << channel << std::endl;
    return true;
    }
    
    return false;
}

bool BinancePublicWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot unsubscribe: not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        
        // Send unsubscription message
        Json::Value unsubscribe_msg;
        unsubscribe_msg["method"] = "UNSUBSCRIBE";
        unsubscribe_msg["params"] = Json::Value(Json::arrayValue);
        unsubscribe_msg["params"].append(channel);
        unsubscribe_msg["id"] = 1;
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, unsubscribe_msg);
        
        if (send_message(message)) {
        std::cout << "[BINANCE] Unsubscribed from channel: " << channel << std::endl;
        return true;
        }
    }
    
    std::cerr << "[BINANCE] Channel not found: " << channel << std::endl;
    return false;
}

std::vector<std::string> BinancePublicWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

bool BinancePublicWebSocketHandler::subscribe_to_orderbook(const std::string& symbol) {
    std::string channel = symbol + "@depth20@100ms";
    return subscribe_to_channel(channel);
}

bool BinancePublicWebSocketHandler::subscribe_to_trades(const std::string& symbol) {
    std::string channel = symbol + "@trade";
    return subscribe_to_channel(channel);
}

bool BinancePublicWebSocketHandler::subscribe_to_ticker(const std::string& symbol) {
    std::string channel = symbol + "@ticker";
    return subscribe_to_channel(channel);
}


void BinancePublicWebSocketHandler::handle_websocket_message(const websocket_transport::WebSocketMessage& message) {
    std::cout << "[BINANCE] Received message: " << message.data << std::endl;
    
    // Parse and handle Binance message
    parse_binance_message(message.data);
    
    // Call user callback
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message.data;
        ws_message.is_binary = message.is_binary;
        ws_message.timestamp_us = message.timestamp_us;
        ws_message.channel = message.channel;
        
        message_callback_(ws_message);
    }
}

void BinancePublicWebSocketHandler::handle_connection_error(int error_code, const std::string& error_message) {
    std::cerr << "[BINANCE] Connection error: " << error_code << " - " << error_message << std::endl;
    
    if (error_callback_) {
        error_callback_(error_message);
    }
}

void BinancePublicWebSocketHandler::handle_connection_status(bool connected) {
    std::cout << "[BINANCE] Connection status: " << (connected ? "connected" : "disconnected") << std::endl;
    
    if (connect_callback_) {
        connect_callback_(connected);
    }
}

void BinancePublicWebSocketHandler::parse_binance_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[BINANCE] Failed to parse JSON message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("stream")) {
            std::string stream = root["stream"].asString();
            Json::Value data = root["data"];
            
            if (stream.find("@depth") != std::string::npos) {
                handle_orderbook_update(data);
            } else if (stream.find("@trade") != std::string::npos) {
                handle_trade_update(data);
            } else if (stream.find("@ticker") != std::string::npos) {
                handle_ticker_update(data);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE] Error parsing message: " << e.what() << std::endl;
    }
}

void BinancePublicWebSocketHandler::handle_orderbook_update(const Json::Value& data) {
    // Parse orderbook update and call appropriate callbacks
    std::cout << "[BINANCE] Orderbook update received" << std::endl;
    
    try {
        // Extract symbol
        std::string symbol = data["s"].asString();
        uint64_t timestamp = data["E"].asUInt64();
        
        // Create OrderBookSnapshot
        proto::OrderBookSnapshot orderbook;
        orderbook.set_symbol(symbol);
        orderbook.set_exch("binance");
        orderbook.set_timestamp_us(timestamp);
        
        // Parse ALL bids
        if (data.isMember("b") && data["b"].isArray()) {
            for (const auto& bid : data["b"]) {
                if (bid.isArray() && bid.size() >= 2) {
                    auto* level = orderbook.add_bids();
                    level->set_price(std::stod(bid[0].asString()));
                    level->set_qty(std::stod(bid[1].asString()));
                }
            }
        }
        
        // Parse ALL asks
        if (data.isMember("a") && data["a"].isArray()) {
            for (const auto& ask : data["a"]) {
                if (ask.isArray() && ask.size() >= 2) {
                    auto* level = orderbook.add_asks();
                    level->set_price(std::stod(ask[0].asString()));
                    level->set_qty(std::stod(ask[1].asString()));
                }
            }
        }
        
        std::cout << "[BINANCE] Parsed orderbook: " << symbol 
                  << " bids: " << orderbook.bids_size() 
                  << " asks: " << orderbook.asks_size() << std::endl;
        
        // TODO: Call appropriate callback to forward orderbook to strategy
        // This would need to be connected to the strategy container
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE] Error parsing orderbook update: " << e.what() << std::endl;
    }
}

void BinancePublicWebSocketHandler::handle_trade_update(const Json::Value& data) {
    // Parse trade update and call appropriate callbacks
    std::cout << "[BINANCE] Trade update received" << std::endl;
    
    // TODO: Implement trade parsing and normalization
    // This would extract price, quantity, timestamp, etc.
}

void BinancePublicWebSocketHandler::handle_ticker_update(const Json::Value& data) {
    // Parse ticker update and call appropriate callbacks
    std::cout << "[BINANCE] Ticker update received" << std::endl;
    
    // TODO: Implement ticker parsing and normalization
    // This would extract price, volume, 24h change, etc.
}

} // namespace binance