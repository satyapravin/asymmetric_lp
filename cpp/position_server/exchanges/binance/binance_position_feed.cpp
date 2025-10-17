#include "binance_position_feed.hpp"
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
#include "binance_lws_client.h" // Reuse the WebSocket client from quote server

// HMAC SHA256 implementation for Binance API signature
namespace binance_pos {
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

// Base64 encoding for WebSocket authentication
std::string base64_encode(const std::string& input) {
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);
  
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, input.c_str(), input.length());
  BIO_flush(bio);
  
  BUF_MEM* buffer_ptr;
  BIO_get_mem_ptr(bio, &buffer_ptr);
  
  std::string result(buffer_ptr->data, buffer_ptr->length);
  BIO_free_all(bio);
  
  return result;
}

BinancePositionFeed::BinancePositionFeed(const std::string& api_key, const std::string& api_secret)
  : api_key_(api_key), api_secret_(api_secret), websocket_url_("wss://fstream.binance.com/ws") {
}

BinancePositionFeed::~BinancePositionFeed() {
  disconnect();
}

bool BinancePositionFeed::connect(const std::string& account) {
  if (connected_.load()) return true;
  
  account_ = account;
  running_.store(true);
  worker_thread_ = std::thread([this]() { this->run_websocket_client(); });
  
  // Wait a bit for connection to establish
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  std::cout << "[BINANCE_POSITION] Connected to account: " << account << std::endl;
  return true;
}

void BinancePositionFeed::disconnect() {
  if (!connected_.load()) return;
  
  running_.store(false);
  runflag_ = 0; // Signal WebSocket client to stop
  
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  connected_.store(false);
  
  std::cout << "[BINANCE_POSITION] Disconnected from account: " << account_ << std::endl;
}

bool BinancePositionFeed::is_connected() const {
  return connected_.load();
}

// Static callback functions for C interface
static void binance_pos_on_open_cb(void* user) {
  BinancePositionFeed* feed = static_cast<BinancePositionFeed*>(user);
  feed->handle_ws_open();
}

static void binance_pos_on_message_cb(void* user, const char* data, int len) {
  BinancePositionFeed* feed = static_cast<BinancePositionFeed*>(user);
  std::string message(data, len);
  feed->handle_ws_message(message);
}

static void binance_pos_on_close_cb(void* user) {
  BinancePositionFeed* feed = static_cast<BinancePositionFeed*>(user);
  feed->handle_ws_close();
}

void BinancePositionFeed::run_websocket_client() {
  // Build authentication message
  std::string auth_message = build_auth_message();
  
  // Set up callbacks
  BinanceLwsCallbacks callbacks;
  callbacks.on_open = binance_pos_on_open_cb;
  callbacks.on_message = binance_pos_on_message_cb;
  callbacks.on_close = binance_pos_on_close_cb;
  
  // Start WebSocket client
  runflag_ = 1;
  binance_lws_run("fstream.binance.com", 443, 1, "/ws", &runflag_, this, callbacks);
}

std::string BinancePositionFeed::build_auth_message() {
  // Generate timestamp
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  
  // Build query string
  std::string query_string = "timestamp=" + std::to_string(now);
  
  // Generate signature
  std::string signature = binance_pos::hmac_sha256(api_secret_, query_string);
  
  // Build auth message
  std::ostringstream oss;
  oss << "{\"method\":\"SUBSCRIBE\",\"params\":[\"!userData\"],\"id\":1}";
  
  return oss.str();
}

void BinancePositionFeed::handle_ws_open() {
  connected_.store(true);
  std::cout << "[BINANCE_POSITION] WebSocket connected" << std::endl;
  
  // Send authentication message
  std::string auth_msg = build_auth_message();
  // Note: The actual sending would be handled by the WebSocket client
  std::cout << "[BINANCE_POSITION] Sent auth message: " << auth_msg << std::endl;
}

void BinancePositionFeed::handle_ws_close() {
  connected_.store(false);
  std::cout << "[BINANCE_POSITION] WebSocket disconnected" << std::endl;
}

void BinancePositionFeed::handle_ws_message(const std::string& message) {
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

void BinancePositionFeed::process_position_update(const std::string& message) {
  try {
    // Parse JSON using simdjson
    simdjson::dom::parser parser;
    simdjson::dom::element root = parser.parse(message);

    // Check if it's an ACCOUNT_UPDATE event
    auto event_type = root["e"];
    if (!event_type.is_string() || std::string(event_type.get_string().value()) != "ACCOUNT_UPDATE") {
      return;
    }

    auto account_data = root["a"];
    if (!account_data.is_object()) {
      return;
    }

    auto positions_array = account_data["P"];
    if (!positions_array.is_array()) {
      return;
    }

    std::lock_guard<std::mutex> lock(positions_mutex_);
    for (auto position_element : positions_array.get_array()) {
      if (!position_element.is_object()) continue;

      auto symbol_field = position_element["s"];
      auto qty_field = position_element["pa"];
      auto avg_price_field = position_element["ep"];

      if (!symbol_field.is_string() || !qty_field.is_string() || !avg_price_field.is_string()) {
        continue;
      }

      std::string symbol = std::string(symbol_field.get_string().value());
      double qty = std::stod(std::string(qty_field.get_string().value()));
      double avg_price = std::stod(std::string(avg_price_field.get_string().value()));

      // Only process symbols we're tracking
      {
        std::lock_guard<std::mutex> symbols_lock(symbols_mutex_);
        if (symbols_.find(symbol) == symbols_.end()) {
          continue;
        }
      }
      
      // Check if position changed
      bool position_changed = false;
      if (current_positions_.find(symbol) == current_positions_.end() ||
          current_positions_[symbol] != qty) {
        position_changed = true;
      }
      
      current_positions_[symbol] = qty;
      avg_prices_[symbol] = avg_price;
      
      // Emit position update if changed
      if (position_changed && on_position_update) {
        on_position_update(symbol, "BINANCE", qty, avg_price);
        std::cout << "[BINANCE_POSITION] " << symbol << " qty=" << qty 
                  << " avg_price=" << avg_price << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cout << "[BINANCE_POSITION] Error processing position update: " << e.what() << std::endl;
  }
}