#include "binance_subscriber.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace binance {

BinanceSubscriber::BinanceSubscriber(const BinanceSubscriberConfig& config) : config_(config) {
    std::cout << "[BINANCE_SUBSCRIBER] Initializing Binance Subscriber" << std::endl;
}

BinanceSubscriber::~BinanceSubscriber() {
    disconnect();
}

bool BinanceSubscriber::connect() {
    std::cout << "[BINANCE_SUBSCRIBER] Connecting to Binance WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[BINANCE_SUBSCRIBER] Already connected" << std::endl;
        return true;
    }
    
    if (!custom_transport_) {
        std::cerr << "[BINANCE_SUBSCRIBER] No WebSocket transport injected!" << std::endl;
        return false;
    }
    
    try {
        // Use injected transport
        if (custom_transport_->connect(config_.websocket_url)) {
            connected_.store(true);
            std::cout << "[BINANCE_SUBSCRIBER] Connected successfully using injected transport" << std::endl;
            return true;
        } else {
            std::cerr << "[BINANCE_SUBSCRIBER] Failed to connect using injected transport" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_SUBSCRIBER] Connection error: " << e.what() << std::endl;
        return false;
    }
}

void BinanceSubscriber::disconnect() {
    std::cout << "[BINANCE_SUBSCRIBER] Disconnecting..." << std::endl;
    
    if (custom_transport_) {
        custom_transport_->disconnect();
    }
    
    connected_.store(false);
    
    std::cout << "[BINANCE_SUBSCRIBER] Disconnected" << std::endl;
}

bool BinanceSubscriber::is_connected() const {
    return connected_.load();
}

bool BinanceSubscriber::subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) {
    if (!is_connected()) {
        std::cerr << "[BINANCE_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string sub_msg = create_subscription_message(binance_symbol, "depth");
    std::cout << "[BINANCE_SUBSCRIBER] Subscribing to orderbook: " << binance_symbol 
              << " top_n: " << top_n << " frequency: " << frequency_ms << "ms" << std::endl;
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), binance_symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(binance_symbol);
        }
    }
    
    // Mock subscription response
    std::string mock_response = R"({"method":"SUBSCRIBE","params":["")" + binance_symbol + R"(@depth@100ms"],"id":)" + std::to_string(request_id_++) + R"(})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool BinanceSubscriber::subscribe_trades(const std::string& symbol) {
    if (!is_connected()) {
        std::cerr << "[BINANCE_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string sub_msg = create_subscription_message(binance_symbol, "trade");
    std::cout << "[BINANCE_SUBSCRIBER] Subscribing to trades: " << binance_symbol << std::endl;
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), binance_symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(binance_symbol);
        }
    }
    
    // Mock subscription response
    std::string mock_response = R"({"method":"SUBSCRIBE","params":["")" + binance_symbol + R"(@trade"],"id":)" + std::to_string(request_id_++) + R"(})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool BinanceSubscriber::unsubscribe(const std::string& symbol) {
    if (!is_connected()) {
        std::cerr << "[BINANCE_SUBSCRIBER] Not connected" << std::endl;
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string unsub_msg = create_unsubscription_message(binance_symbol, "depth");
    std::cout << "[BINANCE_SUBSCRIBER] Unsubscribing from: " << binance_symbol << std::endl;
    
    // Remove from subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), binance_symbol);
        if (it != subscribed_symbols_.end()) {
            subscribed_symbols_.erase(it);
        }
    }
    
    // Mock unsubscription response
    std::string mock_response = R"({"method":"UNSUBSCRIBE","params":["")" + binance_symbol + R"(@depth@100ms"],"id":)" + std::to_string(request_id_++) + R"(})";
    handle_websocket_message(mock_response);
    
    return true;
}

void BinanceSubscriber::set_orderbook_callback(OrderbookCallback callback) {
    orderbook_callback_ = callback;
}

void BinanceSubscriber::set_trade_callback(TradeCallback callback) {
    trade_callback_ = callback;
}

void BinanceSubscriber::websocket_loop() {
    std::cout << "[BINANCE_SUBSCRIBER] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional market data updates
            static int counter = 0;
            if (++counter % 20 == 0) {
                std::string mock_orderbook_update = R"({"stream":"btcusdt@depth@100ms","data":{"e":"depthUpdate","E":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"s":"BTCUSDT","U":123456789,"u":123456790,"b":[["50000.00","0.1"],["49999.00","0.2"]],"a":[["50001.00","0.15"],["50002.00","0.25"]]}})";
                handle_websocket_message(mock_orderbook_update);
            }
            
            if (counter % 35 == 0) {
                std::string mock_trade_update = R"({"stream":"btcusdt@trade","data":{"e":"trade","E":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"s":"BTCUSDT","t":123456789,"p":"50000.50","q":"0.1","b":123456789,"a":123456790,"T":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"m":true,"M":true}})";
                handle_websocket_message(mock_trade_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[BINANCE_SUBSCRIBER] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[BINANCE_SUBSCRIBER] WebSocket loop stopped" << std::endl;
}

void BinanceSubscriber::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[BINANCE_SUBSCRIBER] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("stream") && root.isMember("data")) {
            std::string stream = root["stream"].asString();
            Json::Value data = root["data"];
            
            if (stream.find("@depth") != std::string::npos) {
                handle_orderbook_update(data);
            } else if (stream.find("@trade") != std::string::npos) {
                handle_trade_update(data);
            }
        } else if (root.isMember("method")) {
            // Handle subscription responses
            std::cout << "[BINANCE_SUBSCRIBER] Subscription response: " << message << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_SUBSCRIBER] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void BinanceSubscriber::handle_orderbook_update(const Json::Value& orderbook_data) {
    proto::OrderBookSnapshot orderbook;
    orderbook.set_exch("binance");
    orderbook.set_symbol(orderbook_data["s"].asString());
    orderbook.set_timestamp_us(orderbook_data["E"].asUInt64()); // Keep as milliseconds
    
    // Parse bids
    const Json::Value& bids = orderbook_data["b"];
    if (bids.isArray()) {
        for (const auto& bid : bids) {
            proto::OrderBookLevel* level = orderbook.add_bids();
            level->set_price(std::stod(bid[0].asString()));
            level->set_qty(std::stod(bid[1].asString()));
        }
    }
    
    // Parse asks
    const Json::Value& asks = orderbook_data["a"];
    if (asks.isArray()) {
        for (const auto& ask : asks) {
            proto::OrderBookLevel* level = orderbook.add_asks();
            level->set_price(std::stod(ask[0].asString()));
            level->set_qty(std::stod(ask[1].asString()));
        }
    }
    
    if (orderbook_callback_) {
        orderbook_callback_(orderbook);
    }
    
    std::cout << "[BINANCE_SUBSCRIBER] Orderbook update: " << orderbook.symbol() 
              << " bids: " << orderbook.bids_size() 
              << " asks: " << orderbook.asks_size() << std::endl;
}

void BinanceSubscriber::handle_trade_update(const Json::Value& trade_data) {
    proto::Trade trade;
    trade.set_exch("BINANCE");
    trade.set_symbol(trade_data["s"].asString());
    trade.set_price(std::stod(trade_data["p"].asString()));
    trade.set_qty(std::stod(trade_data["q"].asString()));
    trade.set_is_buyer_maker(trade_data["m"].asBool());
    trade.set_trade_id(trade_data["t"].asString());
    trade.set_timestamp_us(trade_data["T"].asUInt64() * 1000); // Convert to microseconds
    
    if (trade_callback_) {
        trade_callback_(trade);
    }
    
    std::cout << "[BINANCE_SUBSCRIBER] Trade update: " << trade.symbol() 
              << " " << trade.qty() << "@" << trade.price() 
              << " side: " << (trade.is_buyer_maker() ? "SELL" : "BUY") << std::endl;
}

std::string BinanceSubscriber::create_subscription_message(const std::string& symbol, const std::string& channel) {
    Json::Value root;
    root["method"] = "SUBSCRIBE";
    root["id"] = generate_request_id();
    
    Json::Value params(Json::arrayValue);
    if (channel == "depth") {
        params.append(symbol + "@depth@100ms");
    } else if (channel == "trade") {
        params.append(symbol + "@trade");
    }
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string BinanceSubscriber::create_unsubscription_message(const std::string& symbol, const std::string& channel) {
    Json::Value root;
    root["method"] = "UNSUBSCRIBE";
    root["id"] = generate_request_id();
    
    Json::Value params(Json::arrayValue);
    if (channel == "depth") {
        params.append(symbol + "@depth@100ms");
    } else if (channel == "trade") {
        params.append(symbol + "@trade");
    }
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string BinanceSubscriber::generate_request_id() {
    return std::to_string(request_id_++);
}

std::string BinanceSubscriber::convert_symbol_to_binance(const std::string& symbol) {
    // Convert symbol format to Binance format
    // e.g., "BTCUSDT" -> "btcusdt" (lowercase)
    std::string binance_symbol = symbol;
    std::transform(binance_symbol.begin(), binance_symbol.end(), binance_symbol.begin(), ::tolower);
    return binance_symbol;
}

void BinanceSubscriber::set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    std::cout << "[BINANCE_SUBSCRIBER] Setting custom WebSocket transport for testing" << std::endl;
    custom_transport_ = std::move(transport);
}

void BinanceSubscriber::start() {
    std::cout << "[BINANCE_SUBSCRIBER] Starting subscriber" << std::endl;
    
    if (!custom_transport_) {
        std::cerr << "[BINANCE_SUBSCRIBER] No WebSocket transport injected!" << std::endl;
        return;
    }
    
    // Set up message callback to handle incoming messages
    custom_transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& message) {
        std::cout << "[BINANCE_SUBSCRIBER] Received message: " << message.data << std::endl;
        
        // Parse the message and call appropriate handlers
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(message.data, root)) {
            if (root.isMember("stream") && root.isMember("data")) {
                // This is a stream message
                const Json::Value& data = root["data"];
                if (data.isMember("e")) {
                    std::string event_type = data["e"].asString();
                    if (event_type == "depthUpdate") {
                        handle_orderbook_update(data);
                    } else if (event_type == "trade") {
                        handle_trade_update(data);
                    }
                }
            }
        }
    });
    
    // Connect if not already connected
    if (!connected_.load()) {
        connect();
    }
}

void BinanceSubscriber::stop() {
    std::cout << "[BINANCE_SUBSCRIBER] Stopping subscriber" << std::endl;
    disconnect();
}

void BinanceSubscriber::set_error_callback(std::function<void(const std::string&)> callback) {
    std::cout << "[BINANCE_SUBSCRIBER] Setting error callback" << std::endl;
    error_callback_ = callback;
}

} // namespace binance
