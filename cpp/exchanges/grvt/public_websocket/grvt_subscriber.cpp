#include "grvt_subscriber.hpp"
#include "../../../utils/logging/log_helper.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <json/json.h>

namespace grvt {

GrvtSubscriber::GrvtSubscriber(const GrvtSubscriberConfig& config) : config_(config) {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Initializing GRVT Subscriber");
}

GrvtSubscriber::~GrvtSubscriber() {
    disconnect();
}

bool GrvtSubscriber::connect() {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Connecting to GRVT WebSocket...");
    
    if (connected_.load()) {
        LOG_INFO_COMP("GRVT_SUBSCRIBER", "Already connected");
        return true;
    }
    
    try {
        // If custom transport is set, use it (for testing)
        if (custom_transport_) {
            LOG_INFO_COMP("GRVT_SUBSCRIBER", "Using custom WebSocket transport");
            
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
                
                websocket_thread_ = std::thread(&GrvtSubscriber::websocket_loop, this);
                LOG_INFO_COMP("GRVT_SUBSCRIBER", "Connected successfully using injected transport");
                return true;
            } else {
                LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Failed to connect using custom transport");
                return false;
            }
        }
        
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&GrvtSubscriber::websocket_loop, this);
        
        connected_ = true;
        
        LOG_INFO_COMP("GRVT_SUBSCRIBER", "Connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Connection failed: " + std::string(e.what()));
        return false;
    }
}

void GrvtSubscriber::disconnect() {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Disconnecting...");
    
    websocket_running_ = false;
    connected_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Disconnected");
}

bool GrvtSubscriber::is_connected() const {
    return connected_.load();
}

bool GrvtSubscriber::subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) {
    if (!is_connected()) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // GRVT API: Use orderbook.s (snapshot) or orderbook.d (delta) channel
    // Symbol should be separate parameter, not concatenated
    std::string sub_msg = create_subscription_message(symbol, "orderbook", config_.use_snapshot_channels);
    std::string log_msg = "Subscribing to orderbook: " + symbol + 
                          " channel: " + get_channel_name("orderbook", config_.use_snapshot_channels) +
                          " top_n: " + std::to_string(top_n) + " frequency: " + std::to_string(frequency_ms) + "ms";
    LOG_INFO_COMP("GRVT_SUBSCRIBER", log_msg);
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Mock subscription response (GRVT format)
    std::string channel_name = get_channel_name("orderbook", config_.use_snapshot_channels);
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
                                R"(,"result":{"subscribed":true,"channel":")" + channel_name + 
                                R"(","instrument":")" + symbol + R"("}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool GrvtSubscriber::subscribe_trades(const std::string& symbol) {
    if (!is_connected()) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // GRVT API: trades channel doesn't have snapshot/delta variants
    std::string sub_msg = create_subscription_message(symbol, "trades", false);
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Subscribing to trades: " + symbol);
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Mock subscription response (GRVT format)
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
                                R"(,"result":{"subscribed":true,"channel":"trades","instrument":")" + symbol + R"("}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool GrvtSubscriber::subscribe_ticker(const std::string& symbol) {
    if (!is_connected()) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // GRVT API: Use ticker.s (snapshot) or ticker.d (delta) channel
    std::string sub_msg = create_subscription_message(symbol, "ticker", config_.use_snapshot_channels);
    std::string ticker_log_msg = "Subscribing to ticker: " + symbol + 
                                  " channel: " + get_channel_name("ticker", config_.use_snapshot_channels);
    LOG_INFO_COMP("GRVT_SUBSCRIBER", ticker_log_msg);
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Mock subscription response (GRVT format)
    std::string channel_name = get_channel_name("ticker", config_.use_snapshot_channels);
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
                                R"(,"result":{"subscribed":true,"channel":")" + channel_name + 
                                R"(","instrument":")" + symbol + R"("}})";
    handle_websocket_message(mock_response);
    
    return true;
}

bool GrvtSubscriber::unsubscribe(const std::string& symbol) {
    if (!is_connected()) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // Unsubscribe from all channels for this symbol
    // In practice, you might want to track which channels are subscribed per symbol
    std::string unsub_msg = create_unsubscription_message(symbol, "orderbook", config_.use_snapshot_channels);
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Unsubscribing from: " + symbol);
    
    // Remove from subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it != subscribed_symbols_.end()) {
            subscribed_symbols_.erase(it);
        }
    }
    
    // Mock unsubscription response (GRVT format)
    std::string channel_name = get_channel_name("orderbook", config_.use_snapshot_channels);
    std::string mock_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
                                R"(,"result":{"unsubscribed":true,"channel":")" + channel_name + 
                                R"(","instrument":")" + symbol + R"("}})";
    handle_websocket_message(mock_response);
    
    return true;
}

void GrvtSubscriber::set_orderbook_callback(OrderbookCallback callback) {
    orderbook_callback_ = callback;
}

void GrvtSubscriber::set_trade_callback(TradeCallback callback) {
    trade_callback_ = callback;
}

void GrvtSubscriber::websocket_loop() {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "WebSocket loop started");
    
    // If custom transport is set, messages will come via callback
    // Just wait for the loop to be stopped
    if (custom_transport_) {
        LOG_INFO_COMP("GRVT_SUBSCRIBER", "Using custom transport - messages will arrive via callback");
        while (websocket_running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        // Fallback: Mock WebSocket message processing
        while (websocket_running_) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Simulate occasional market data updates
                static int counter = 0;
                if (++counter % 20 == 0) {
                    std::string mock_orderbook_update = R"({"jsonrpc":"2.0","method":"orderbook_update","params":{"symbol":"BTCUSDT","bids":[["50000.0","0.1"],["49999.0","0.2"]],"asks":[["50001.0","0.15"],["50002.0","0.25"]],"timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                    handle_websocket_message(mock_orderbook_update);
                }
                
                if (counter % 35 == 0) {
                    std::string mock_trade_update = R"({"jsonrpc":"2.0","method":"trade_update","params":{"symbol":"BTCUSDT","price":50000.5,"quantity":0.1,"side":"BUY","timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                    handle_websocket_message(mock_trade_update);
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR_COMP("GRVT_SUBSCRIBER", "WebSocket loop error: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "WebSocket loop stopped");
}

void GrvtSubscriber::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Failed to parse WebSocket message");
            return;
        }
        
        // Handle both full and lite variants
        bool is_lite = root.isMember("m") || root.isMember("j");
        std::string method_key = is_lite ? "m" : "method";
        std::string params_key = is_lite ? "p" : "params";
        std::string result_key = is_lite ? "r" : "result";
        std::string error_key = is_lite ? "e" : "error";
        
        // Handle different message types
        if (root.isMember(method_key)) {
            std::string method = root[method_key].asString();
            
            // GRVT API: Method names match channel names (e.g., "orderbook.s", "ticker.d", "trades")
            if (root.isMember(params_key)) {
                const Json::Value& params = root[params_key];
                
                if (method.find("orderbook") != std::string::npos) {
                    // Params format: [channel, instrument, data]
                    // Data contains bids/asks arrays
                    if (params.isArray() && params.size() >= 3) {
                        Json::Value orderbook_data;
                        orderbook_data["symbol"] = params[1];  // instrument
                        
                        // Parse orderbook data (format depends on full/lite variant)
                        const Json::Value& data = params[2];
                        
                        // Extract timestamp from data if present, otherwise use current time
                        if (data.isMember("timestamp")) {
                            orderbook_data["timestamp"] = data["timestamp"];
                        } else if (data.isMember("t")) {
                            orderbook_data["timestamp"] = data["t"];
                        } else {
                            orderbook_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                        }
                        
                        // Copy bids/asks arrays (check each separately for full/lite variants)
                        if (data.isMember("bids")) {
                            orderbook_data["bids"] = data["bids"];
                        } else if (data.isMember("b")) {
                            orderbook_data["bids"] = data["b"];
                        }
                        if (data.isMember("asks")) {
                            orderbook_data["asks"] = data["asks"];
                        } else if (data.isMember("a")) {
                            orderbook_data["asks"] = data["a"];
                        }
                        
                        handle_orderbook_update(orderbook_data);
                    }
                } else if (method.find("trades") != std::string::npos || method.find("trade") != std::string::npos) {
                    // Params format: [channel, instrument, trade_data]
                    if (params.isArray() && params.size() >= 3) {
                        const Json::Value& trade_data = params[2];
                        Json::Value trade_json;
                        trade_json["symbol"] = params[1];
                        trade_json["price"] = trade_data.isMember("price") ? trade_data["price"] : trade_data["p"];
                        trade_json["quantity"] = trade_data.isMember("quantity") ? trade_data["quantity"] : trade_data["q"];
                        trade_json["side"] = trade_data.isMember("side") ? trade_data["side"] : trade_data["s"];
                        trade_json["tradeId"] = trade_data.isMember("tradeId") ? trade_data["tradeId"] : trade_data["ti"];
                        trade_json["timestamp"] = trade_data.isMember("timestamp") ? trade_data["timestamp"] : trade_data["t"];
                        
                        handle_trade_update(trade_json);
                    }
                } else if (method.find("ticker") != std::string::npos) {
                    // Handle ticker updates if needed
                    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Ticker update received: " + message);
                }
            }
        } else if (root.isMember(result_key)) {
            // Handle subscription/unsubscription responses
            LOG_INFO_COMP("GRVT_SUBSCRIBER", "Subscription response: " + message);
        } else if (root.isMember(error_key)) {
            // Handle error responses
            std::string error_msg = root[error_key].isString() ? 
                root[error_key].asString() : root[error_key].toStyledString();
            LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Error response: " + error_msg);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Error handling WebSocket message: " + std::string(e.what()));
    }
}

void GrvtSubscriber::handle_orderbook_update(const Json::Value& orderbook_data) {
    proto::OrderBookSnapshot orderbook;
    orderbook.set_exch("GRVT");
    
    if (!orderbook_data.isMember("symbol")) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Orderbook data missing symbol");
        return;
    }
    orderbook.set_symbol(orderbook_data["symbol"].asString());
    
    // Handle timestamp (could be in milliseconds or nanoseconds)
    if (orderbook_data.isMember("timestamp")) {
        uint64_t timestamp = orderbook_data["timestamp"].asUInt64();
        // If timestamp is very large (> year 2100 in ms), assume it's nanoseconds
        if (timestamp > 4102444800000ULL) {
            orderbook.set_timestamp_us(timestamp / 1000); // Convert nanoseconds to microseconds
        } else {
            orderbook.set_timestamp_us(timestamp * 1000); // Convert milliseconds to microseconds
        }
    } else {
        orderbook.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    // Parse bids
    if (orderbook_data.isMember("bids")) {
        const Json::Value& bids = orderbook_data["bids"];
        if (bids.isArray()) {
            for (const auto& bid : bids) {
                if (bid.isArray() && bid.size() >= 2) {
                    proto::OrderBookLevel* level = orderbook.add_bids();
                    // Handle string or numeric values
                    if (bid[0].isString()) {
                        level->set_price(std::stod(bid[0].asString()));
                    } else {
                        level->set_price(bid[0].asDouble());
                    }
                    if (bid[1].isString()) {
                        level->set_qty(std::stod(bid[1].asString()));
                    } else {
                        level->set_qty(bid[1].asDouble());
                    }
                }
            }
        }
    }
    
    // Parse asks
    if (orderbook_data.isMember("asks")) {
        const Json::Value& asks = orderbook_data["asks"];
        if (asks.isArray()) {
            for (const auto& ask : asks) {
                if (ask.isArray() && ask.size() >= 2) {
                    proto::OrderBookLevel* level = orderbook.add_asks();
                    // Handle string or numeric values
                    if (ask[0].isString()) {
                        level->set_price(std::stod(ask[0].asString()));
                    } else {
                        level->set_price(ask[0].asDouble());
                    }
                    if (ask[1].isString()) {
                        level->set_qty(std::stod(ask[1].asString()));
                    } else {
                        level->set_qty(ask[1].asDouble());
                    }
                }
            }
        }
    }
    
    std::string orderbook_log_msg = "Orderbook update: " + orderbook.symbol() + 
                                     " bids: " + std::to_string(orderbook.bids_size()) + 
                                     " asks: " + std::to_string(orderbook.asks_size());
    LOG_DEBUG_COMP("GRVT_SUBSCRIBER", orderbook_log_msg);
    
    if (orderbook_callback_) {
        orderbook_callback_(orderbook);
    }
}

void GrvtSubscriber::handle_trade_update(const Json::Value& trade_data) {
    proto::Trade trade;
    trade.set_exch("GRVT");
    
    if (!trade_data.isMember("symbol")) {
        LOG_ERROR_COMP("GRVT_SUBSCRIBER", "Trade data missing symbol");
        return;
    }
    trade.set_symbol(trade_data["symbol"].asString());
    
    // Handle price (could be string or numeric)
    if (trade_data.isMember("price")) {
        if (trade_data["price"].isString()) {
            trade.set_price(std::stod(trade_data["price"].asString()));
        } else {
            trade.set_price(trade_data["price"].asDouble());
        }
    } else if (trade_data.isMember("p")) {
        if (trade_data["p"].isString()) {
            trade.set_price(std::stod(trade_data["p"].asString()));
        } else {
            trade.set_price(trade_data["p"].asDouble());
        }
    }
    
    // Handle quantity (could be string or numeric)
    if (trade_data.isMember("quantity")) {
        if (trade_data["quantity"].isString()) {
            trade.set_qty(std::stod(trade_data["quantity"].asString()));
        } else {
            trade.set_qty(trade_data["quantity"].asDouble());
        }
    } else if (trade_data.isMember("q")) {
        if (trade_data["q"].isString()) {
            trade.set_qty(std::stod(trade_data["q"].asString()));
        } else {
            trade.set_qty(trade_data["q"].asDouble());
        }
    }
    
    // Handle side
    std::string side_str;
    if (trade_data.isMember("side")) {
        side_str = trade_data["side"].asString();
    } else if (trade_data.isMember("s")) {
        side_str = trade_data["s"].asString();
    }
    trade.set_is_buyer_maker(side_str == "sell" || side_str == "SELL");
    
    // Handle trade ID
    if (trade_data.isMember("tradeId")) {
        trade.set_trade_id(trade_data["tradeId"].asString());
    } else if (trade_data.isMember("ti")) {
        trade.set_trade_id(trade_data["ti"].asString());
    }
    
    // Handle timestamp
    if (trade_data.isMember("timestamp")) {
        uint64_t timestamp = trade_data["timestamp"].asUInt64();
        if (timestamp > 4102444800000ULL) {
            trade.set_timestamp_us(timestamp / 1000);
        } else {
            trade.set_timestamp_us(timestamp * 1000);
        }
    } else if (trade_data.isMember("t")) {
        uint64_t timestamp = trade_data["t"].asUInt64();
        if (timestamp > 4102444800000ULL) {
            trade.set_timestamp_us(timestamp / 1000);
        } else {
            trade.set_timestamp_us(timestamp * 1000);
        }
    } else {
        trade.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    std::string trade_log_msg = "Trade update: " + trade.symbol() + 
                                 " " + std::to_string(trade.qty()) + "@" + std::to_string(trade.price()) + 
                                 " side: " + (trade.is_buyer_maker() ? "SELL" : "BUY");
    LOG_DEBUG_COMP("GRVT_SUBSCRIBER", trade_log_msg);
    
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

std::string GrvtSubscriber::get_channel_name(const std::string& base_channel, bool use_snapshot) const {
    // GRVT API: Channels use .s (snapshot) or .d (delta) suffix
    // Trades channel doesn't have snapshot/delta variants
    if (base_channel == "trades") {
        return "trades";
    }
    return base_channel + (use_snapshot ? ".s" : ".d");
}

std::string GrvtSubscriber::create_subscription_message(const std::string& symbol, const std::string& channel, bool use_snapshot) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "SUBSCRIBE";
    
    // GRVT API: params is array with [channel_name, instrument]
    // Channel name includes .s or .d suffix (e.g., "orderbook.s", "ticker.d")
    Json::Value params(Json::arrayValue);
    std::string channel_name = get_channel_name(channel, use_snapshot);
    params.append(channel_name);
    params.append(symbol);  // Instrument name (e.g., "ETH_USDT_Perp")
    
    root["params"] = params;
    
    // Use lite variant if configured
    if (config_.use_lite_version) {
        // Lite variant uses shortened field names: j, m, p, i instead of jsonrpc, method, params, id
        Json::Value lite_root;
        lite_root["j"] = "2.0";
        lite_root["m"] = "SUBSCRIBE";
        lite_root["p"] = params;
        lite_root["i"] = root["id"];
        Json::StreamWriterBuilder builder;
        return Json::writeString(builder, lite_root);
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtSubscriber::create_unsubscription_message(const std::string& symbol, const std::string& channel, bool use_snapshot) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "UNSUBSCRIBE";
    
    // GRVT API: params is array with [channel_name, instrument]
    Json::Value params(Json::arrayValue);
    std::string channel_name = get_channel_name(channel, use_snapshot);
    params.append(channel_name);
    params.append(symbol);
    
    root["params"] = params;
    
    // Use lite variant if configured
    if (config_.use_lite_version) {
        Json::Value lite_root;
        lite_root["j"] = "2.0";
        lite_root["m"] = "UNSUBSCRIBE";
        lite_root["p"] = params;
        lite_root["i"] = root["id"];
        Json::StreamWriterBuilder builder;
        return Json::writeString(builder, lite_root);
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtSubscriber::generate_request_id() {
    return std::to_string(request_id_++);
}

void GrvtSubscriber::start() {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Starting subscriber");
}

void GrvtSubscriber::stop() {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Stopping subscriber");
    
    // Stop custom transport event loop if running
    if (custom_transport_ && custom_transport_->is_event_loop_running()) {
        custom_transport_->stop_event_loop();
    }
    
    disconnect();
}

void GrvtSubscriber::set_error_callback(std::function<void(const std::string&)> callback) {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Setting error callback");
    // Store callback for later use
}

void GrvtSubscriber::set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    LOG_INFO_COMP("GRVT_SUBSCRIBER", "Setting custom WebSocket transport for testing");
    custom_transport_ = std::move(transport);
}

} // namespace grvt
