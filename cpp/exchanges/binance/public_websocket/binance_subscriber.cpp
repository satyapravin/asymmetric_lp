#include "binance_subscriber.hpp"
#include "../../../utils/logging/logger.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace binance {

BinanceSubscriber::BinanceSubscriber(const BinanceSubscriberConfig& config) : config_(config) {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.info("Initializing Binance Subscriber");
}

BinanceSubscriber::~BinanceSubscriber() {
    disconnect();
}

bool BinanceSubscriber::connect() {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.info("Connecting to Binance WebSocket...");
    
    if (connected_.load()) {
        logger.debug("Already connected");
        return true;
    }
    
    if (!custom_transport_) {
        logger.error("No WebSocket transport injected!");
        return false;
    }
    
    try {
        // Use injected transport
        if (custom_transport_->connect(config_.websocket_url)) {
            connected_.store(true);
            logger.info("Connected successfully using injected transport");
            return true;
        } else {
            logger.error("Failed to connect using injected transport");
            return false;
        }
    } catch (const std::exception& e) {
        logger.error("Connection error: " + std::string(e.what()));
        return false;
    }
}

void BinanceSubscriber::disconnect() {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.info("Disconnecting...");
    
    if (custom_transport_) {
        custom_transport_->disconnect();
    }
    
    connected_.store(false);
    
    logger.info("Disconnected");
}

bool BinanceSubscriber::is_connected() const {
    return connected_.load();
}

bool BinanceSubscriber::subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    if (!is_connected()) {
        logger.error("Not connected");
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string sub_msg = create_subscription_message(binance_symbol, "depth");
    logger.info("Subscribing to orderbook: " + binance_symbol + 
               " top_n: " + std::to_string(top_n) + 
               " frequency: " + std::to_string(frequency_ms) + "ms");
    
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
    logging::Logger logger("BINANCE_SUBSCRIBER");
    if (!is_connected()) {
        logger.error("Not connected");
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string sub_msg = create_subscription_message(binance_symbol, "trade");
    logger.info("Subscribing to trades: " + binance_symbol);
    
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
    logging::Logger logger("BINANCE_SUBSCRIBER");
    if (!is_connected()) {
        logger.error("Not connected");
        return false;
    }
    
    std::string binance_symbol = convert_symbol_to_binance(symbol);
    std::string unsub_msg = create_unsubscription_message(binance_symbol, "depth");
    logger.debug("Unsubscribing from: " + binance_symbol);
    
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
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.debug("WebSocket loop started");
    
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
            logger.error("WebSocket loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    logger.debug("WebSocket loop stopped");
}

void BinanceSubscriber::handle_websocket_message(const std::string& message) {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            logger.error("Failed to parse WebSocket message");
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
            logger.debug("Subscription response: " + message);
        }
        
    } catch (const std::exception& e) {
        logger.error("Error handling WebSocket message: " + std::string(e.what()));
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
    
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.debug("Orderbook update: " + orderbook.symbol() + 
                " bids: " + std::to_string(orderbook.bids_size()) + 
                " asks: " + std::to_string(orderbook.asks_size()));
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
    
    logging::Logger logger("BINANCE_SUBSCRIBER");
    std::stringstream ss;
    ss << "Trade update: " << trade.symbol() << " " << trade.qty() << "@" << trade.price() 
       << " side: " << (trade.is_buyer_maker() ? "SELL" : "BUY");
    logger.debug(ss.str());
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
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.debug("Setting custom WebSocket transport for testing");
    custom_transport_ = std::move(transport);
}

void BinanceSubscriber::start() {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.info("Starting subscriber");
    
    if (!custom_transport_) {
        logger.error("No WebSocket transport injected!");
        return;
    }
    
    // Set up message callback to handle incoming messages
    custom_transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& message) {
        logging::Logger callback_logger("BINANCE_SUBSCRIBER");
        callback_logger.debug("Received message: " + message.data);
        
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
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.info("Stopping subscriber");
    disconnect();
}

void BinanceSubscriber::set_error_callback(std::function<void(const std::string&)> callback) {
    logging::Logger logger("BINANCE_SUBSCRIBER");
    logger.debug("Setting error callback");
    error_callback_ = callback;
}

} // namespace binance
