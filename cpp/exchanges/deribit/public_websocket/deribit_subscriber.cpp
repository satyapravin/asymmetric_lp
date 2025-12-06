#include "deribit_subscriber.hpp"
#include "../../../utils/logging/log_helper.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <json/json.h>

namespace deribit {

DeribitSubscriber::DeribitSubscriber(const DeribitSubscriberConfig& config) : config_(config) {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Initializing Deribit Subscriber");
}

DeribitSubscriber::~DeribitSubscriber() {
    disconnect();
}

bool DeribitSubscriber::connect() {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Connecting to Deribit WebSocket...");
    
    if (connected_.load()) {
        LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Already connected");
        return true;
    }
    
    try {
        // If custom transport is set, use it (for testing)
        if (custom_transport_) {
            LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Using custom WebSocket transport");
            
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
                
                websocket_thread_ = std::thread(&DeribitSubscriber::websocket_loop, this);
                LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Connected successfully using injected transport");
                return true;
            } else {
                LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Failed to connect using custom transport");
                return false;
            }
        }
        
        // Initialize WebSocket connection (mock implementation for now)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitSubscriber::websocket_loop, this);
        
        connected_ = true;
        
        LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Connection failed: " + std::string(e.what()));
        return false;
    }
}

void DeribitSubscriber::disconnect() {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Disconnecting...");
    
    websocket_running_ = false;
    connected_ = false;
    
    // Wait for websocket thread to finish first
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    // Stop custom transport event loop after thread has joined
    if (custom_transport_) {
        custom_transport_->stop_event_loop();
    }
    
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Disconnected");
}

bool DeribitSubscriber::is_connected() const {
    return connected_.load();
}

bool DeribitSubscriber::subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) {
    if (!is_connected()) {
        LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // Deribit API: book.{instrument_name}.{interval}
    // Interval can be "raw", "100ms", "1s", etc.
    std::string interval = get_interval_string(frequency_ms);
    std::string sub_msg = create_subscription_message(symbol, "book", interval);
    std::string log_msg = "Subscribing to orderbook: " + symbol + " top_n: " + std::to_string(top_n) + 
                          " frequency: " + std::to_string(frequency_ms) + "ms (interval: " + interval + ")";
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", log_msg);
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Note: Subscription messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    return true;
}

bool DeribitSubscriber::subscribe_trades(const std::string& symbol) {
    if (!is_connected()) {
        LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // Deribit API: trades.{instrument_name}.raw
    std::string sub_msg = create_subscription_message(symbol, "trades", "raw");
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Subscribing to trades: " + symbol);
    
    // Add to subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it == subscribed_symbols_.end()) {
            subscribed_symbols_.push_back(symbol);
        }
    }
    
    // Note: Subscription messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the message would be sent here
    
    return true;
}

bool DeribitSubscriber::unsubscribe(const std::string& symbol) {
    if (!is_connected()) {
        LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Not connected");
        return false;
    }
    
    // Unsubscribe from both book and trades channels
    std::string unsub_msg_book = create_unsubscription_message(symbol, "book", "raw");
    std::string unsub_msg_trades = create_unsubscription_message(symbol, "trades", "raw");
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Unsubscribing from: " + symbol);
    
    // Remove from subscribed symbols
    {
        std::lock_guard<std::mutex> lock(symbols_mutex_);
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it != subscribed_symbols_.end()) {
            subscribed_symbols_.erase(it);
        }
    }
    
    // Note: Unsubscription messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the messages would be sent here
    
    return true;
}

void DeribitSubscriber::set_orderbook_callback(OrderbookCallback callback) {
    orderbook_callback_ = callback;
}

void DeribitSubscriber::set_trade_callback(TradeCallback callback) {
    trade_callback_ = callback;
}

void DeribitSubscriber::set_error_callback(std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Setting error callback");
}

void DeribitSubscriber::websocket_loop() {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "WebSocket loop started");
    
    if (custom_transport_) {
        LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Using custom transport - messages will arrive via callback");
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
                
                // Simulate occasional market data updates (only for mock mode)
                static int counter = 0;
                if (++counter % 20 == 0) {
                    std::string mock_orderbook_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"book.BTC-PERPETUAL.raw","data":{"bids":[["50000.0","0.1"],["49999.0","0.2"]],"asks":[["50001.0","0.15"],["50002.0","0.25"]],"timestamp":)" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(,"change_id":)" + std::to_string(counter) + R"(}}})";
                    handle_websocket_message(mock_orderbook_update);
                }
                
                if (counter % 35 == 0) {
                    std::string mock_trade_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"trades.BTC-PERPETUAL.raw","data":[{"price":50000.5,"amount":0.1,"direction":"buy","timestamp":)" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(,"trade_id":"trade_)" + std::to_string(counter) + R"(","trade_seq":)" + std::to_string(counter) + R"(}]}})";
                    handle_websocket_message(mock_trade_update);
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "WebSocket loop error: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "WebSocket loop stopped");
}

void DeribitSubscriber::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Failed to parse WebSocket message");
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "subscription" && root.isMember("params")) {
                Json::Value params = root["params"];
                std::string channel = params["channel"].asString();
                
                // Extract symbol from channel (e.g., "book.BTC-PERPETUAL.raw" -> "BTC-PERPETUAL")
                std::string symbol;
                size_t first_dot = channel.find('.');
                size_t second_dot = channel.find('.', first_dot + 1);
                if (first_dot != std::string::npos && second_dot != std::string::npos) {
                    symbol = channel.substr(first_dot + 1, second_dot - first_dot - 1);
                }
                
                if (channel.find("book.") == 0 && params.isMember("data")) {
                    handle_orderbook_update(params["data"], symbol);
                } else if (channel.find("trades.") == 0 && params.isMember("data")) {
                    handle_trade_update(params["data"], symbol);
                }
            }
        } else if (root.isMember("result")) {
            // Handle subscription responses
            LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Subscription response: " + message);
        } else if (root.isMember("error")) {
            // Handle errors
            std::string error_msg = "Deribit API error: " + root["error"].toStyledString();
            LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", error_msg);
            if (error_callback_) {
                error_callback_(error_msg);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("DERIBIT_SUBSCRIBER", "Error handling WebSocket message: " + std::string(e.what()));
        if (error_callback_) {
            error_callback_(std::string("Error parsing message: ") + e.what());
        }
    }
}

void DeribitSubscriber::handle_orderbook_update(const Json::Value& orderbook_data, const std::string& symbol) {
    proto::OrderBookSnapshot orderbook;
    orderbook.set_exch("DERIBIT");
    orderbook.set_symbol(symbol.empty() ? "BTC-PERPETUAL" : symbol);
    
    // Deribit orderbook format: {"bids":[[price,qty],...],"asks":[[price,qty],...],"timestamp":...,"change_id":...}
    if (orderbook_data.isMember("timestamp")) {
        // Deribit timestamp is in milliseconds
        orderbook.set_timestamp_us(orderbook_data["timestamp"].asUInt64() * 1000); // Convert to microseconds
    } else {
        orderbook.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    // Parse bids
    if (orderbook_data.isMember("bids") && orderbook_data["bids"].isArray()) {
        const Json::Value& bids = orderbook_data["bids"];
        for (const auto& bid : bids) {
            if (bid.isArray() && bid.size() >= 2) {
                proto::OrderBookLevel* level = orderbook.add_bids();
                // Handle both string and numeric values
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
    
    // Parse asks
    if (orderbook_data.isMember("asks") && orderbook_data["asks"].isArray()) {
        const Json::Value& asks = orderbook_data["asks"];
        for (const auto& ask : asks) {
            if (ask.isArray() && ask.size() >= 2) {
                proto::OrderBookLevel* level = orderbook.add_asks();
                // Handle both string and numeric values
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
    
    if (orderbook_callback_) {
        orderbook_callback_(orderbook);
    }
    
    std::string log_msg = "Orderbook update: " + orderbook.symbol() + 
                          " bids: " + std::to_string(orderbook.bids_size()) + 
                          " asks: " + std::to_string(orderbook.asks_size());
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", log_msg);
}

void DeribitSubscriber::handle_trade_update(const Json::Value& trade_data, const std::string& symbol) {
    // Deribit trades format: array of trade objects or single trade object
    Json::Value trades_array;
    if (trade_data.isArray()) {
        trades_array = trade_data;
    } else {
        trades_array.append(trade_data);
    }
    
    for (const auto& trade_obj : trades_array) {
        proto::Trade trade;
        trade.set_exch("DERIBIT");
        trade.set_symbol(symbol.empty() ? "BTC-PERPETUAL" : symbol);
        
        // Handle both string and numeric values
        if (trade_obj.isMember("price")) {
            if (trade_obj["price"].isString()) {
                trade.set_price(std::stod(trade_obj["price"].asString()));
            } else {
                trade.set_price(trade_obj["price"].asDouble());
            }
        }
        
        if (trade_obj.isMember("amount")) {
            if (trade_obj["amount"].isString()) {
                trade.set_qty(std::stod(trade_obj["amount"].asString()));
            } else {
                trade.set_qty(trade_obj["amount"].asDouble());
            }
        }
        
        if (trade_obj.isMember("direction")) {
            std::string direction = trade_obj["direction"].asString();
            trade.set_is_buyer_maker(direction == "sell"); // If direction is "sell", buyer is maker
        }
        
        if (trade_obj.isMember("trade_id")) {
            trade.set_trade_id(trade_obj["trade_id"].asString());
        } else if (trade_obj.isMember("trade_seq")) {
            trade.set_trade_id("trade_" + std::to_string(trade_obj["trade_seq"].asUInt64()));
        }
        
        if (trade_obj.isMember("timestamp")) {
            // Deribit timestamp is in milliseconds
            trade.set_timestamp_us(trade_obj["timestamp"].asUInt64() * 1000); // Convert to microseconds
        } else {
            trade.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        
        if (trade_callback_) {
            trade_callback_(trade);
        }
        
        std::string log_msg = "Trade update: " + trade.symbol() + 
                              " " + std::to_string(trade.qty()) + "@" + std::to_string(trade.price()) + 
                              " side: " + (trade.is_buyer_maker() ? "SELL" : "BUY");
        LOG_INFO_COMP("DERIBIT_SUBSCRIBER", log_msg);
    }
}

std::string DeribitSubscriber::create_subscription_message(const std::string& symbol, const std::string& channel, const std::string& interval) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "public/subscribe";
    
    Json::Value params;
    Json::Value channels(Json::arrayValue);
    std::string channel_name = channel + "." + symbol + "." + interval;
    channels.append(channel_name);
    params["channels"] = channels;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitSubscriber::create_unsubscription_message(const std::string& symbol, const std::string& channel, const std::string& interval) {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "public/unsubscribe";
    
    Json::Value params;
    Json::Value channels(Json::arrayValue);
    std::string channel_name = channel + "." + symbol + "." + interval;
    channels.append(channel_name);
    params["channels"] = channels;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitSubscriber::generate_request_id() {
    return std::to_string(request_id_++);
}

std::string DeribitSubscriber::get_interval_string(int frequency_ms) {
    if (frequency_ms <= 0) {
        return "raw";
    } else if (frequency_ms < 100) {
        return "raw";
    } else if (frequency_ms < 1000) {
        return std::to_string(frequency_ms) + "ms";
    } else {
        return std::to_string(frequency_ms / 1000) + "s";
    }
}

void DeribitSubscriber::start() {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Starting subscriber");
}

void DeribitSubscriber::stop() {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Stopping subscriber");
    disconnect();
}

void DeribitSubscriber::set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) {
    LOG_INFO_COMP("DERIBIT_SUBSCRIBER", "Setting custom WebSocket transport for testing");
    custom_transport_ = std::move(transport);
}

} // namespace deribit
