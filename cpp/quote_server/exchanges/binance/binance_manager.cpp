#include "binance_manager.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <iostream>
#include <simdjson.h>
#include "../../utils/config/config.hpp"
#include <cstring>
#include "binance_lws_client.h"
#include <atomic>
#include <sstream>
#ifdef PROTO_ENABLED
#include "market_data.pb.h"
#endif

BinanceManager::BinanceManager(const std::string& websocket_url)
  : websocket_url_(websocket_url) {}

BinanceManager::~BinanceManager() { stop(); }

static void on_open_thunk(void* user) { static_cast<BinanceManager*>(user)->handle_ws_open(); }
static void on_message_thunk(void* user, const char* data, int len) { static_cast<BinanceManager*>(user)->handle_ws_message(std::string(data, len)); }
static void on_close_thunk(void* user) { static_cast<BinanceManager*>(user)->handle_ws_close(); }

bool BinanceManager::start() {
  if (running_.load()) return true;
  running_.store(true);
  emit_connection(true);
  worker_ = std::thread([this]{
    // Build combined stream path: /stream?streams=s1/s2
    std::vector<std::string> streams;
    {
      std::lock_guard<std::mutex> lock(subs_mutex_);
      for (const auto& sym : subs_) {
        if (channels_.empty()) {
          streams.push_back(sym + "@depth" + std::to_string(book_depth_) + "@100ms");
          streams.push_back(sym + "@trade");
        } else {
          for (const auto& ch : channels_) streams.push_back(sym + "@" + ch);
        }
      }
    }
    std::string path = "/stream?streams=";
    for (size_t i = 0; i < streams.size(); ++i) {
      path += streams[i];
      if (i + 1 < streams.size()) path += "/";
    }

    std::string host = "fstream.binance.com";
    int port = 443;
    bool use_ssl = true;

    runflag_ = 1;
    BinanceLwsCallbacks cbs{&on_open_thunk, &on_message_thunk, &on_close_thunk};
    binance_lws_run(host.c_str(), port, use_ssl ? 1 : 0, path.c_str(), &runflag_, this, cbs);
  });
  return true;
}

void BinanceManager::stop() {
  if (!running_.load()) return;
  running_.store(false);
  runflag_ = 0;
  if (worker_.joinable()) worker_.join();
  emit_connection(false);
}

void BinanceManager::subscribe_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(subs_mutex_);
  subs_.push_back(symbol);
}

void BinanceManager::unsubscribe_symbol(const std::string& symbol) {
  subs_.erase(std::remove(subs_.begin(), subs_.end(), symbol), subs_.end());
}


void BinanceManager::handle_ws_open() {
  std::cout << "[BINANCE] WebSocket connected" << std::endl;
  emit_connection(true);
}

void BinanceManager::handle_ws_close() {
  std::cout << "[BINANCE] WebSocket disconnected" << std::endl;
  emit_connection(false);
}

void BinanceManager::handle_ws_message(const std::string& message) {
  // Append to buffer
  message_buffer_ += message;
  
  // Try to parse complete messages from buffer
  size_t pos = 0;
  while (pos < message_buffer_.length()) {
    // Look for complete JSON objects (starting with { and ending with })
    size_t start = message_buffer_.find('{', pos);
    if (start == std::string::npos) {
      // No more complete messages, keep remaining buffer
      message_buffer_ = message_buffer_.substr(pos);
      break;
    }
    
    // Find matching closing brace
    int brace_count = 0;
    size_t end = start;
    for (size_t i = start; i < message_buffer_.length(); i++) {
      if (message_buffer_[i] == '{') brace_count++;
      else if (message_buffer_[i] == '}') {
        brace_count--;
        if (brace_count == 0) {
          end = i;
          break;
        }
      }
    }
    
    if (brace_count == 0) {
      // Found complete message
      std::string complete_message = message_buffer_.substr(start, end - start + 1);
      process_complete_message(complete_message);
      pos = end + 1;
    } else {
      // Incomplete message, keep buffer
      message_buffer_ = message_buffer_.substr(start);
      break;
    }
  }
}

void BinanceManager::process_complete_message(const std::string& message) {
  try {
    // Debug: Print first few messages to see format
    static int debug_count = 0;
    if (debug_count < 3) {
      std::cout << "[BINANCE] DEBUG Complete Message " << debug_count << " (length=" << message.length() << "): " 
                << message.substr(0, std::min(message.length(), size_t(200))) << std::endl;
      debug_count++;
    }
    
    // Parse JSON using simdjson
    simdjson::dom::parser parser;
    simdjson::dom::element doc = parser.parse(message);
    
    // Extract stream name
    std::string stream = std::string(doc["stream"].get_string().value());
    
    // Extract data object
    simdjson::dom::object data = doc["data"].get_object();
    std::string event_type = std::string(data["e"].get_string().value());
    std::string symbol = std::string(data["s"].get_string().value());
    uint64_t timestamp_us = data["E"].get_uint64().value() * 1000; // Convert ms to us
    
    if (event_type == "depthUpdate") {
      process_orderbook_message(data, symbol, timestamp_us);
    } else if (event_type == "trade") {
      process_trade_message(data, symbol, timestamp_us);
    }
    
  } catch (const std::exception& e) {
    std::cout << "[BINANCE] Error processing complete message: " << e.what() << std::endl;
    // Debug: Print problematic message
    static int error_count = 0;
    if (error_count < 2) {
      std::cout << "[BINANCE] DEBUG Error message " << error_count << " (length=" << message.length() << "): " 
                << message.substr(0, std::min(message.length(), size_t(200))) << std::endl;
      error_count++;
    }
  }
}

void BinanceManager::process_orderbook_message(const simdjson::dom::object& data, 
                                              const std::string& symbol, 
                                              uint64_t timestamp_us) {
  try {
    std::cout << "[BINANCE] Processing orderbook for " << symbol << " at " << timestamp_us << std::endl;
    
    // For now, use text format to verify both orderbook and trade feeds work
    // TODO: Enable Protocol Buffers once descriptor conflict is resolved
    // Fallback to simple text format
    std::ostringstream oss;
    oss << "BINANCE_ORDERBOOK " << symbol << " TIMESTAMP:" << timestamp_us;
    
    simdjson::dom::array bids = data["b"].get_array();
    oss << " BIDS:";
    for (auto bid : bids) {
      simdjson::dom::array bid_array = bid.get_array();
      oss << " " << std::string(bid_array.at(0).get_string().value()) << "@" << std::string(bid_array.at(1).get_string().value());
    }
    
    simdjson::dom::array asks = data["a"].get_array();
    oss << " ASKS:";
    for (auto ask : asks) {
      simdjson::dom::array ask_array = ask.get_array();
      oss << " " << std::string(ask_array.at(0).get_string().value()) << "@" << std::string(ask_array.at(1).get_string().value());
    }
    
    emit_message(oss.str());
    
  } catch (const std::exception& e) {
    std::cout << "[BINANCE] Error processing orderbook: " << e.what() << std::endl;
  }
}

void BinanceManager::process_trade_message(const simdjson::dom::object& data, 
                                          const std::string& symbol, 
                                          uint64_t timestamp_us) {
  try {
    // Debug: Print first few trade messages to see structure
    static int trade_debug_count = 0;
    if (trade_debug_count < 2) {
      std::cout << "[BINANCE] DEBUG Trade message " << trade_debug_count << " for " << symbol << std::endl;
      trade_debug_count++;
    }
    
    // Check if required fields exist and have correct types
    auto price_field = data["p"];
    auto qty_field = data["q"];
    auto maker_field = data["m"];
    auto trade_id_field = data["t"];
    
    if (!price_field.is_string() || !qty_field.is_string() || 
        !maker_field.is_bool() || !trade_id_field.is_number()) {
      std::cout << "[BINANCE] Trade message missing required fields or wrong types" << std::endl;
      return;
    }
    
    // For now, use text format to verify trade feed works
    // TODO: Enable Protocol Buffers once descriptor conflict is resolved
    std::string price = std::string(price_field.get_string().value());
    std::string qty = std::string(qty_field.get_string().value());
    bool is_buyer_maker = maker_field.get_bool().value();
    uint64_t trade_id = trade_id_field.get_uint64().value();
    
    std::ostringstream oss;
    oss << "BINANCE_TRADE " << symbol << " " << (is_buyer_maker ? "SELL" : "BUY") 
        << " " << qty << "@" << price << " ID:" << trade_id << " TIMESTAMP:" << timestamp_us;
    
    emit_message(oss.str());
    
  } catch (const std::exception& e) {
    std::cout << "[BINANCE] Error processing trade: " << e.what() << std::endl;
  }
}

extern "C" {
  BinanceManager* create_exchange_manager(const char* websocket_url) {
    return new BinanceManager(websocket_url ? std::string(websocket_url) : std::string());
  }
  void destroy_exchange_manager(BinanceManager* mgr) {
    delete mgr;
  }
}
