#include "binance_exchange_manager.hpp"
#include <iostream>
#include <json/json.h> // You'd use a proper JSON library
#include <chrono>

BinanceExchangeManager::BinanceExchangeManager(const std::string& websocket_url)
    : websocket_url_(websocket_url) {
  loop_ = uv_loop_new();
}

BinanceExchangeManager::~BinanceExchangeManager() {
  stop();
  uv_loop_close(loop_);
}

bool BinanceExchangeManager::start() {
  if (running_.load()) return true;
  
  running_.store(true);
  event_loop_thread_ = std::thread([this]() { this->run_event_loop(); });
  
  std::cout << "[BINANCE_MANAGER] Starting Binance websocket to " << websocket_url_ << std::endl;
  
  return true;
}

void BinanceExchangeManager::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  if (connected_.load()) {
    uv_close((uv_handle_t*)&websocket_, nullptr);
  }
  
  // Stop event loop
  uv_async_t stop_async;
  uv_async_init(loop_, &stop_async, [](uv_async_t* handle) {
    uv_stop(handle->loop);
  });
  uv_async_send(&stop_async);
  
  if (event_loop_thread_.joinable()) {
    event_loop_thread_.join();
  }
  
  std::cout << "[BINANCE_MANAGER] Stopped" << std::endl;
}

void BinanceExchangeManager::subscribe_symbol(const std::string& symbol) {
  // Binance uses lowercase symbols
  std::string binance_symbol = symbol;
  std::transform(binance_symbol.begin(), binance_symbol.end(), binance_symbol.begin(), ::tolower);
  
  subscribe_depth_stream(binance_symbol);
}

void BinanceExchangeManager::unsubscribe_symbol(const std::string& symbol) {
  std::string binance_symbol = symbol;
  std::transform(binance_symbol.begin(), binance_symbol.end(), binance_symbol.begin(), ::tolower);
  
  std::string stream_name = binance_symbol + "@depth";
  
  // Remove from subscribed streams
  subscribed_streams_.erase(
    std::remove(subscribed_streams_.begin(), subscribed_streams_.end(), stream_name),
    subscribed_streams_.end()
  );
  
  if (connected_.load()) {
    // Send unsubscription request
    std::string unsubscribe_msg = R"({"method":"UNSUBSCRIBE","params":[")" + stream_name + R"("],"id":)" + std::to_string(request_id_++) + "}";
    uv_websocket_send(&websocket_, unsubscribe_msg.c_str(), unsubscribe_msg.length());
    std::cout << "[BINANCE_MANAGER] Unsubscribed from " << stream_name << std::endl;
  }
}

void BinanceExchangeManager::subscribe_depth_stream(const std::string& symbol) {
  std::string stream_name = symbol + "@depth";
  
  // Add to subscribed streams
  if (std::find(subscribed_streams_.begin(), subscribed_streams_.end(), stream_name) == subscribed_streams_.end()) {
    subscribed_streams_.push_back(stream_name);
  }
  
  if (connected_.load()) {
    send_subscription_request(stream_name);
  }
}

void BinanceExchangeManager::subscribe_trade_stream(const std::string& symbol) {
  std::string stream_name = symbol + "@trade";
  
  if (std::find(subscribed_streams_.begin(), subscribed_streams_.end(), stream_name) == subscribed_streams_.end()) {
    subscribed_streams_.push_back(stream_name);
  }
  
  if (connected_.load()) {
    send_subscription_request(stream_name);
  }
}

void BinanceExchangeManager::subscribe_kline_stream(const std::string& symbol, const std::string& interval) {
  std::string stream_name = symbol + "@kline_" + interval;
  
  if (std::find(subscribed_streams_.begin(), subscribed_streams_.end(), stream_name) == subscribed_streams_.end()) {
    subscribed_streams_.push_back(stream_name);
  }
  
  if (connected_.load()) {
    send_subscription_request(stream_name);
  }
}

void BinanceExchangeManager::connect_to_binance() {
  uv_websocket_init(loop_, &websocket_);
  
  websocket_.on_connect = on_connect;
  websocket_.on_message = on_message;
  websocket_.on_close = on_close;
  websocket_.on_error = on_error;
  websocket_.data = this;
  
  // Parse URL and connect
  // In real implementation, parse websocket_url_ and extract host/port
  uv_tcp_connect(&connect_req_, (uv_tcp_t*)&websocket_, 
                uv_ip4_addr("127.0.0.1", 8080), on_connect);
}

void BinanceExchangeManager::handle_binance_message(const std::string& message) {
  // Binance-specific message handling
  try {
    // Parse JSON message
    // Json::Value root;
    // Json::Reader reader;
    // if (reader.parse(message, root)) {
    //   if (root.isMember("stream")) {
    //     std::string stream = root["stream"].asString();
    //     if (stream.find("@depth") != std::string::npos) {
    //       // Handle depth update
    //       std::cout << "[BINANCE_MANAGER] Depth update for " << stream << std::endl;
    //     } else if (stream.find("@trade") != std::string::npos) {
    //       // Handle trade update
    //       std::cout << "[BINANCE_MANAGER] Trade update for " << stream << std::endl;
    //     }
    //   }
    // }
    
    // For now, just forward the message
    if (message_callback_) {
      message_callback_(message);
    }
  } catch (const std::exception& e) {
    std::cerr << "[BINANCE_MANAGER] Error parsing message: " << e.what() << std::endl;
  }
}

void BinanceExchangeManager::send_subscription_request(const std::string& stream_name) {
  std::string subscribe_msg = R"({"method":"SUBSCRIBE","params":[")" + stream_name + R"("],"id":)" + std::to_string(request_id_++) + "}";
  uv_websocket_send(&websocket_, subscribe_msg.c_str(), subscribe_msg.length());
  std::cout << "[BINANCE_MANAGER] Subscribed to " << stream_name << std::endl;
}

void BinanceExchangeManager::run_event_loop() {
  connect_to_binance();
  uv_run(loop_, UV_RUN_DEFAULT);
}

// Static libuv callbacks
void BinanceExchangeManager::on_connect(uv_connect_t* req, int status) {
  BinanceExchangeManager* manager = static_cast<BinanceExchangeManager*>(req->handle->data);
  
  if (status < 0) {
    std::cerr << "[BINANCE_MANAGER] Connection failed: " << uv_strerror(status) << std::endl;
    manager->connected_.store(false);
    if (manager->connection_callback_) {
      manager->connection_callback_(false);
    }
    return;
  }
  
  std::cout << "[BINANCE_MANAGER] Connected to Binance" << std::endl;
  manager->connected_.store(true);
  
  // Resubscribe to all streams
  for (const auto& stream : manager->subscribed_streams_) {
    manager->send_subscription_request(stream);
  }
  
  if (manager->connection_callback_) {
    manager->connection_callback_(true);
  }
}

void BinanceExchangeManager::on_message(uv_websocket_t* handle, const char* data, size_t len) {
  BinanceExchangeManager* manager = static_cast<BinanceExchangeManager*>(handle->data);
  
  std::string message(data, len);
  manager->handle_binance_message(message);
}

void BinanceExchangeManager::on_close(uv_websocket_t* handle) {
  BinanceExchangeManager* manager = static_cast<BinanceExchangeManager*>(handle->data);
  
  std::cout << "[BINANCE_MANAGER] Connection closed" << std::endl;
  manager->connected_.store(false);
  if (manager->connection_callback_) {
    manager->connection_callback_(false);
  }
}

void BinanceExchangeManager::on_error(uv_websocket_t* handle, int error) {
  BinanceExchangeManager* manager = static_cast<BinanceExchangeManager*>(handle->data);
  
  std::cerr << "[BINANCE_MANAGER] Error: " << uv_strerror(error) << std::endl;
  manager->connected_.store(false);
  if (manager->connection_callback_) {
    manager->connection_callback_(false);
  }
}
