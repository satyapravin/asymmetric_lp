#include "deribit_subscriber.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace deribit {

DeribitSubscriber::DeribitSubscriber(const DeribitSubscriberConfig& config) : config_(config) {
    std::cout << "[DERIBIT_SUBSCRIBER] Initializing Deribit Subscriber" << std::endl;
}

DeribitSubscriber::~DeribitSubscriber() {
    disconnect();
}

bool DeribitSubscriber::connect() {
    std::cout << "[DERIBIT_SUBSCRIBER] Connecting to Deribit WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[DERIBIT_SUBSCRIBER] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitSubscriber::websocket_loop, this);
        
        connected_ = true;
        
        std::cout << "[DERIBIT_SUBSCRIBER] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_SUBSCRIBER] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void DeribitSubscriber::disconnect() {
    std::cout << "[DERIBIT_SUBSCRIBER] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    connected_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    std::cout << "[DERIBIT_SUBSCRIBER] Disconnected" << std::endl;
}

bool DeribitSubscriber::is_connected() const {
    return connected_.load();
}

bool DeribitSubscriber::subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) {
    if (!is_connected()) {
        std::cerr << "[DERIBIT_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string sub_msg = create_subscription_message(symbol, "book");
    std::cout << "[DERIBIT_SUBSCRIBER] Subscribing to orderbook: " << symbol 
              << " top_n: " << top_n << " frequency: " << frequency_ms << "ms" << std::endl;
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Mock subscription response
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"subscribed":true,"channel":"book.)" + symbol + R"(.raw"}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool DeribitSubscriber::subscribe_trades(const std::string& symbol) {
    if (!is_connected()) {
        std::cerr << "[DERIBIT_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string sub_msg = create_subscription_message(symbol, "trades");
    std::cout << "[DERIBIT_SUBSCRIBER] Subscribing to trades: " << symbol << std::endl;
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Mock subscription response
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"subscribed":true,"channel":"trades.)" + symbol + R"(.raw"}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool DeribitSubscriber::unsubscribe(const std::string& symbol) {
    if (!is_connected()) {
        std::cerr << "[DERIBIT_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string unsub_msg = create_unsubscription_message(symbol, "book");
    std::cout << "[DERIBIT_SUBSCRIBER] Unsubscribing from: " << symbol << std::endl;
    
    // Remove from subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it != subscribed_symbols_.end()) {
            subscribed_symbols_.erase(it);
        }
    }
    
    // Mock unsubscription response
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"unsubscribed":true,"channel":"book.)" + symbol + R"(.raw"}})";
    handle_websocket_message(mock_response);
    
    return true;
}

void DeribitSubscriber::set_orderbook_callback(OrderbookCallback callback) {
    orderbook_callback_ = callback;
}

void DeribitSubscriber::set_trade_callback(TradeCallback callback) {
    trade_callback_ = callback;
}

void DeribitSubscriber::websocket_loop() {
    std::cout << "[DERIBIT_SUBSCRIBER] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional market data updates
            static int counter = 0;
            if (++counter % 20 == 0) {
                std::string mock_orderbook_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"book.BTC-PERPETUAL.raw","data":{"bids":[["50000.0","0.1"],["49999.0","0.2"]],"asks":[["50001.0","0.15"],["50002.0","0.25"]],"timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}}})";
                handle_websocket_message(mock_orderbook_update);
            }
            
            if (counter % 35 == 0) {
                std::string mock_trade_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"trades.BTC-PERPETUAL.raw","data":{"price":50000.5,"amount":0.1,"direction":"buy","timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}}})";
                handle_websocket_message(mock_trade_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DERIBIT_SUBSCRIBER] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[DERIBIT_SUBSCRIBER] WebSocket loop stopped" << std::endl;
}

void DeribitSubscriber::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[DERIBIT_SUBSCRIBER] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "subscription" && root.isMember("params")) {
                Json::Value params = root["params"];
                std::string channel = params["channel"].asString();
                
                if (channel.find("book.") == 0 && params.isMember("data")) {
                    handle_orderbook_update(params["data"]);
                } else if (channel.find("trades.") == 0 && params.isMember("data")) {
                    handle_trade_update(params["data"]);
                }
            }
        } else if (root.isMember("result")) {
            // Handle subscription responses
            std::cout << "[DERIBIT_SUBSCRIBER] Subscription response: " << message << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_SUBSCRIBER] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void DeribitSubscriber::handle_orderbook_update(const Json::Value& orderbook_data) {
    proto::OrderBookSnapshot orderbook;
    orderbook.set_exch("DERIBIT");
    orderbook.set_symbol("BTC-PERPETUAL"); // Extract from channel if needed
    orderbook.set_timestamp_us(orderbook_data["timestamp"].asUInt64() * 1000); // Convert to microseconds
    
    // Parse bids
    const Json::Value& bids = orderbook_data["bids"];
    if (bids.isArray()) {
        for (const auto& bid : bids) {
            proto::OrderBookLevel* level = orderbook.add_bids();
            level->set_price(bid[0].asDouble());
            level->set_qty(bid[1].asDouble());
        }
    }
    
    // Parse asks
    const Json::Value& asks = orderbook_data["asks"];
    if (asks.isArray()) {
        for (const auto& ask : asks) {
            proto::OrderBookLevel* level = orderbook.add_asks();
            level->set_price(ask[0].asDouble());
            level->set_qty(ask[1].asDouble());
        }
    }
    
    if (orderbook_callback_) {
        orderbook_callback_(orderbook);
    }
    
    std::cout << "[DERIBIT_SUBSCRIBER] Orderbook update: " << orderbook.symbol() 
              << " bids: " << orderbook.bids_size() 
              << " asks: " << orderbook.asks_size() << std::endl;
}

void DeribitSubscriber::handle_trade_update(const Json::Value& trade_data) {
    proto::Trade trade;
    trade.set_exch("DERIBIT");
    trade.set_symbol("BTC-PERPETUAL"); // Extract from channel if needed
    trade.set_price(trade_data["price"].asDouble());
    trade.set_qty(trade_data["amount"].asDouble());
    trade.set_is_buyer_maker(trade_data["direction"].asString() == "sell");
    trade.set_trade_id(trade_data["trade_id"].asString());
    trade.set_timestamp_us(trade_data["timestamp"].asUInt64() * 1000); // Convert to microseconds
    
    if (trade_callback_) {
        trade_callback_(trade);
    }
    
    std::cout << "[DERIBIT_SUBSCRIBER] Trade update: " << trade.symbol() 
              << " " << trade.qty() << "@" << trade.price() 
              << " side: " << (trade.is_buyer_maker() ? "SELL" : "BUY") << std::endl;
}

std::string DeribitSubscriber::create_subscription_message(const std::string& symbol, const std::string& channel) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "public/subscribe";
    
    Json::Value params(Json::arrayValue);
    params.append(channel + "." + symbol + ".raw");
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitSubscriber::create_unsubscription_message(const std::string& symbol, const std::string& channel) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "public/unsubscribe";
    
    Json::Value params(Json::arrayValue);
    params.append(channel + "." + symbol + ".raw");
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitSubscriber::generate_request_id() {
    return std::to_string(request_id_++);
}

} // namespace deribit
