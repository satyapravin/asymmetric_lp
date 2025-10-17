#include "deribit_position_feed.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <simdjson.h>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "deribit_lws_client.h" // Reuse the WebSocket client from quote server

// HMAC SHA256 implementation for Deribit API signature
namespace deribit_pos {
std::string hmac_sha256(const std::string& key, const std::string& data) {
  unsigned char* result;
  unsigned int len = 32;
  result = (unsigned char*)malloc(sizeof(char) * len);
  
  HMAC_CTX* ctx = HMAC_CTX_new();
  HMAC_Init_ex(ctx, key.c_str(), key.length(), EVP_sha256(), NULL);
  HMAC_Update(ctx, (unsigned char*)data.c_str(), data.length());
  HMAC_Final(ctx, result, &len);
  HMAC_CTX_free(ctx);
  
  std::stringstream ss;
  for (int i = 0; i < len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
  }
  free(result);
  return ss.str();
}
}

DeribitPositionFeed::DeribitPositionFeed(const std::string& client_id, const std::string& client_secret)
  : client_id_(client_id), client_secret_(client_secret), websocket_url_("wss://www.deribit.com/ws/api/v2") {
}

DeribitPositionFeed::~DeribitPositionFeed() {
  disconnect();
}

bool DeribitPositionFeed::connect(const std::string& account) {
  if (connected_.load()) return true;
  
  account_ = account;
  running_.store(true);
  worker_thread_ = std::thread([this]() { this->run_websocket_client(); });
  
  // Wait a bit for connection to establish
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  std::cout << "[DERIBIT_POSITION] Connected to account: " << account << std::endl;
  return true;
}

void DeribitPositionFeed::disconnect() {
  if (!connected_.load()) return;
  
  running_.store(false);
  runflag_ = 0; // Signal WebSocket client to stop
  
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  connected_.store(false);
  
  std::cout << "[DERIBIT_POSITION] Disconnected from account: " << account_ << std::endl;
}

bool DeribitPositionFeed::is_connected() const {
  return connected_.load();
}

// Static callback functions for C interface
static void deribit_pos_on_open_cb(void* user) {
  DeribitPositionFeed* feed = static_cast<DeribitPositionFeed*>(user);
  feed->handle_ws_open();
}

static void deribit_pos_on_message_cb(void* user, const char* data, int len) {
  DeribitPositionFeed* feed = static_cast<DeribitPositionFeed*>(user);
  std::string message(data, len);
  feed->handle_ws_message(message);
}

static void deribit_pos_on_close_cb(void* user) {
  DeribitPositionFeed* feed = static_cast<DeribitPositionFeed*>(user);
  feed->handle_ws_close();
}

void DeribitPositionFeed::run_websocket_client() {
  // Set up callbacks
  DeribitLwsCallbacks callbacks;
  callbacks.on_open = deribit_pos_on_open_cb;
  callbacks.on_message = deribit_pos_on_message_cb;
  callbacks.on_close = deribit_pos_on_close_cb;
  
  // Start WebSocket client
  runflag_ = 1;
  deribit_lws_run("www.deribit.com", 443, 1, "/ws/api/v2", &runflag_, this, callbacks);
}

std::string DeribitPositionFeed::build_auth_message() {
  // Generate timestamp
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  
  // Build auth message for Deribit
  std::ostringstream oss;
  oss << "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"public/auth\",\"params\":{"
      << "\"grant_type\":\"client_credentials\","
      << "\"client_id\":\"" << client_id_ << "\","
      << "\"client_secret\":\"" << client_secret_ << "\""
      << "}}";
  
  return oss.str();
}

std::string DeribitPositionFeed::build_subscription_message() {
  // Subscribe to user portfolio changes
  std::ostringstream oss;
  oss << "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"private/subscribe\",\"params\":{"
      << "\"channels\":[\"user.portfolio.BTC\",\"user.portfolio.ETH\"]"
      << "}}";
  
  return oss.str();
}

void DeribitPositionFeed::handle_ws_open() {
  connected_.store(true);
  std::cout << "[DERIBIT_POSITION] WebSocket connected" << std::endl;
  
  // Send authentication message
  std::string auth_msg = build_auth_message();
  // Note: The actual sending would be handled by the WebSocket client
  std::cout << "[DERIBIT_POSITION] Sent auth message: " << auth_msg << std::endl;
  
  // Wait a bit then send subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  std::string sub_msg = build_subscription_message();
  std::cout << "[DERIBIT_POSITION] Sent subscription message: " << sub_msg << std::endl;
}

void DeribitPositionFeed::handle_ws_close() {
  connected_.store(false);
  std::cout << "[DERIBIT_POSITION] WebSocket disconnected" << std::endl;
}

void DeribitPositionFeed::handle_ws_message(const std::string& message) {
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
      process_position_update(complete_message);
      pos = end + 1;
    } else {
      // Incomplete message, keep buffer
      message_buffer_ = message_buffer_.substr(start);
      break;
    }
  }
}

void DeribitPositionFeed::process_position_update(const std::string& message) {
  try {
    // Parse JSON using simdjson
    simdjson::dom::parser parser;
    simdjson::dom::element root = parser.parse(message);

    // Check if this is a portfolio update
    auto method_field = root["method"];
    if (!method_field.is_string() || std::string(method_field.get_string().value()) != "subscription") {
      return;
    }

    auto params_field = root["params"];
    if (!params_field.is_object()) {
      return;
    }

    auto data_field = params_field["data"];
    if (!data_field.is_object()) {
      return;
    }

    auto portfolio_field = data_field["portfolio"];
    if (!portfolio_field.is_object()) {
      return;
    }

    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    // Process each currency in portfolio
    for (auto [currency_key, currency_value] : portfolio_field.get_object()) {
      if (!currency_value.is_object()) continue;

      auto total_pl_field = currency_value["total_pl"];
      auto equity_field = currency_value["equity"];

      if (!total_pl_field.is_number() || !equity_field.is_number()) {
        continue;
      }

      double total_pl = total_pl_field.get_double().value();
      double equity = equity_field.get_double().value();
      
      // Deribit doesn't provide individual position quantities like Binance
      // We'll track the portfolio value changes instead
      std::string symbol = std::string(currency_key) + "-PERPETUAL"; // Convert to symbol format
      
      // Only process symbols we're tracking
      {
        std::lock_guard<std::mutex> symbols_lock(symbols_mutex_);
        if (symbols_.find(symbol) == symbols_.end()) {
          continue;
        }
      }
      
      // Check if portfolio value changed significantly
      bool portfolio_changed = false;
      if (current_positions_.find(symbol) == current_positions_.end() ||
          std::abs(current_positions_[symbol] - equity) > 0.01) {
        portfolio_changed = true;
      }
      
      current_positions_[symbol] = equity;
      avg_prices_[symbol] = total_pl; // Use P&L as a proxy for avg price
      
      // Emit position update if changed
      if (portfolio_changed && on_position_update) {
        // For Deribit, we'll use equity as quantity and P&L as price
        on_position_update(symbol, "DERIBIT", equity, total_pl);
        std::cout << "[DERIBIT_POSITION] " << symbol << " equity=" << equity 
                  << " pnl=" << total_pl << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cout << "[DERIBIT_POSITION] Error processing position update: " << e.what() << std::endl;
  }
}