#include "deribit_manager.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <json/json.h>
#include "deribit_lws_client.h"

DeribitManager::DeribitManager(const std::string& websocket_url)
  : websocket_url_(websocket_url) {}

DeribitManager::~DeribitManager() { stop(); }

static void on_open_thunk(void* user) { static_cast<DeribitManager*>(user)->handle_ws_open(); }
static void on_message_thunk(void* user, const char* data, int len) { static_cast<DeribitManager*>(user)->handle_ws_message(std::string(data, len)); }
static void on_close_thunk(void* user) { static_cast<DeribitManager*>(user)->handle_ws_close(); }

bool DeribitManager::start() {
  if (running_.load()) return true;
  running_.store(true);
  emit_connection(true);
  
  worker_ = std::thread([this]{
    // Parse WebSocket URL to extract host, port, path
    std::string host = "www.deribit.com";
    int port = 443;
    bool use_ssl = true;
    std::string path = "/ws/api/v2";
    
    // Override with configured URL if provided
    if (!websocket_url_.empty()) {
      // Simple URL parsing for wss://host:port/path format
      if (websocket_url_.find("wss://") == 0) {
        auto url = websocket_url_.substr(6); // Remove wss://
        auto slash_pos = url.find('/');
        if (slash_pos != std::string::npos) {
          path = url.substr(slash_pos);
          url = url.substr(0, slash_pos);
        }
        auto colon_pos = url.find(':');
        if (colon_pos != std::string::npos) {
          host = url.substr(0, colon_pos);
          port = std::stoi(url.substr(colon_pos + 1));
        } else {
          host = url;
        }
      }
    }

    runflag_ = 1;
    DeribitLwsCallbacks cbs{&on_open_thunk, &on_message_thunk, &on_close_thunk};
    deribit_lws_run(host.c_str(), port, use_ssl ? 1 : 0, path.c_str(), &runflag_, this, cbs);
  });
  return true;
}

void DeribitManager::stop() {
  if (!running_.load()) return;
  running_.store(false);
  runflag_ = 0;
  if (worker_.joinable()) worker_.join();
  emit_connection(false);
}

void DeribitManager::subscribe_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(subs_mutex_);
  subs_.push_back(symbol);
}

void DeribitManager::unsubscribe_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(subs_mutex_);
  subs_.erase(std::remove(subs_.begin(), subs_.end(), symbol), subs_.end());
}

void DeribitManager::configure_kv(const std::vector<std::pair<std::string, std::string>>& kv) {
  parse_config(kv);
}

void DeribitManager::parse_config(const std::vector<std::pair<std::string, std::string>>& kv) {
  for (const auto& [key, val] : kv) {
    if (key == "WEBSOCKET_URL") {
      websocket_url_ = val;
    } else if (key == "CHANNEL") {
      channels_.push_back(val);
    } else if (key == "SYMBOL") {
      subs_.push_back(val);
    } else if (key == "BOOK_DEPTH") {
      try { book_depth_ = std::stoi(val); } catch (...) {}
    } else if (key == "SNAPSHOT_ONLY") {
      snapshot_only_ = (val == "true" || val == "1");
    }
  }
}

void DeribitManager::handle_ws_open() {
  std::cout << "[DERIBIT] WebSocket connected" << std::endl;
  
  // Send subscription message
  std::string sub_msg = build_subscription_message();
  if (!sub_msg.empty()) {
    std::cout << "[DERIBIT] Sending subscription: " << sub_msg << std::endl;
    // Note: In a real implementation, we'd send this via the WebSocket
    // For now, we'll emit it as a message to show the subscription format
    emit_message("DERIBIT_SUBSCRIPTION: " + sub_msg);
  } else {
    std::cout << "[DERIBIT] No symbols configured for subscription" << std::endl;
  }
}

void DeribitManager::handle_ws_close() {
  std::cout << "[DERIBIT] WebSocket disconnected" << std::endl;
}

void DeribitManager::handle_ws_message(const std::string& message) {
  try {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
      std::cout << "[DERIBIT] Failed to parse JSON: " << message.substr(0, 100) << std::endl;
      return;
    }
    
    // Check if this is a subscription notification
    if (root.isMember("method") && root["method"].asString() == "subscription") {
      std::string channel = root["params"]["channel"].asString();
      
      if (channel.find("book.") == 0) {
        process_orderbook_message(message);
      } else if (channel.find("trades.") == 0) {
        process_trade_message(message);
      }
    }
    // Check if this is a direct orderbook response
    else if (root.isMember("result") && root["result"].isMember("bids")) {
      process_orderbook_message(message);
    }
    // Check if this is a trades response
    else if (root.isMember("result") && root["result"].isArray()) {
      process_trade_message(message);
    }
    
    // Emit raw message for debugging
    emit_message("DERIBIT_RAW: " + message.substr(0, std::min(message.length(), size_t(200))));
    
  } catch (const std::exception& e) {
    std::cout << "[DERIBIT] Error processing message: " << e.what() << std::endl;
  }
}

std::string DeribitManager::build_subscription_message() {
  std::vector<std::string> channels;
  
  std::lock_guard<std::mutex> lock(subs_mutex_);
  
  std::cout << "[DERIBIT] Building subscription for " << subs_.size() << " symbols" << std::endl;
  for (const auto& symbol : subs_) {
    std::cout << "[DERIBIT] Symbol: " << symbol << std::endl;
  }
  
  for (const auto& symbol : subs_) {
    if (channels_.empty()) {
      // Default channels: orderbook and trades
      channels.push_back("book." + symbol + ".raw");
      channels.push_back("trades." + symbol + ".raw");
    } else {
      // Use configured channels
      for (const auto& ch : channels_) {
        channels.push_back(ch);
      }
    }
  }
  
  if (channels.empty()) return "";
  
  std::cout << "[DERIBIT] Channels to subscribe: ";
  for (const auto& ch : channels) {
    std::cout << ch << " ";
  }
  std::cout << std::endl;
  
  Json::Value root;
  root["jsonrpc"] = api_version_;
  root["id"] = request_id_++;
  root["method"] = "public/subscribe";
  
  Json::Value params(Json::arrayValue);
  for (const auto& ch : channels) {
    params.append(ch);
  }
  root["params"] = params;
  
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, root);
}

void DeribitManager::process_orderbook_message(const std::string& message) {
  try {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
      return;
    }
    
    Json::Value result;
    if (root.isMember("params") && root["params"].isMember("data")) {
      result = root["params"]["data"];
    } else if (root.isMember("result")) {
      result = root["result"];
    } else {
      return;
    }
    
    if (!result.isMember("bids") || !result.isMember("asks")) {
      return;
    }
    
    std::string symbol = result.get("instrument_name", "UNKNOWN").asString();
    
    // Convert Deribit orderbook to our format
    std::vector<std::pair<double, double>> bids, asks;
    
    // Process bids
    for (const auto& bid : result["bids"]) {
      if (bid.isArray() && bid.size() >= 2) {
        double price = bid[0].asDouble();
        double qty = bid[1].asDouble();
        bids.emplace_back(price, qty);
      }
    }
    
    // Process asks
    for (const auto& ask : result["asks"]) {
      if (ask.isArray() && ask.size() >= 2) {
        double price = ask[0].asDouble();
        double qty = ask[1].asDouble();
        asks.emplace_back(price, qty);
      }
    }
    
    // Emit normalized orderbook
    std::ostringstream oss;
    oss << "DERIBIT_ORDERBOOK " << symbol << " BIDS:";
    for (const auto& [price, qty] : bids) {
      oss << " " << price << "@" << qty;
    }
    oss << " ASKS:";
    for (const auto& [price, qty] : asks) {
      oss << " " << price << "@" << qty;
    }
    
    emit_message(oss.str());
    
  } catch (const std::exception& e) {
    std::cout << "[DERIBIT] Error processing orderbook: " << e.what() << std::endl;
  }
}

void DeribitManager::process_trade_message(const std::string& message) {
  try {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message, root)) {
      return;
    }
    
    Json::Value result;
    if (root.isMember("params") && root["params"].isMember("data")) {
      result = root["params"]["data"];
    } else if (root.isMember("result")) {
      result = root["result"];
    } else {
      return;
    }
    
    std::string symbol = result.get("instrument_name", "UNKNOWN").asString();
    double price = result.get("price", 0.0).asDouble();
    double qty = result.get("amount", 0.0).asDouble();
    std::string direction = result.get("direction", "unknown").asString();
    
    std::ostringstream oss;
    oss << "DERIBIT_TRADE " << symbol << " " << direction << " " << qty << "@" << price;
    
    emit_message(oss.str());
    
  } catch (const std::exception& e) {
    std::cout << "[DERIBIT] Error processing trade: " << e.what() << std::endl;
  }
}

extern "C" {
  DeribitManager* create_exchange_manager(const char* websocket_url) {
    return new DeribitManager(websocket_url ? std::string(websocket_url) : std::string());
  }
  void destroy_exchange_manager(DeribitManager* mgr) {
    delete mgr;
  }
}