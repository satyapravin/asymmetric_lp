#include "exchange_manager.hpp"
#include <iostream>
#include <chrono>

ExchangeManager::ExchangeManager(const std::string& exchange_name, 
                               const std::string& websocket_url)
    : exchange_name_(exchange_name), websocket_url_(websocket_url) {
  loop_ = uv_loop_new();
}

ExchangeManager::~ExchangeManager() {
  stop();
  uv_loop_close(loop_);
}

bool ExchangeManager::start() {
  if (running_.load()) return true;
  
  running_.store(true);
  event_loop_thread_ = std::thread([this]() { this->run_event_loop(); });
  
  std::cout << "[EXCHANGE_MANAGER] Starting " << exchange_name_ 
            << " websocket to " << websocket_url_ << std::endl;
  
  return true;
}

void ExchangeManager::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  // Close websocket
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
  
  std::cout << "[EXCHANGE_MANAGER] Stopped " << exchange_name_ << std::endl;
}

void ExchangeManager::subscribe_symbol(const std::string& symbol) {
  subscribed_symbols_.push_back(symbol);
  
  if (connected_.load()) {
    // Send subscription message (exchange-specific)
    std::string subscribe_msg;
    if (exchange_name_ == "BINANCE") {
      subscribe_msg = R"({"method":"SUBSCRIBE","params":[")" + symbol + R"(@depth"],"id":1})";
    } else if (exchange_name_ == "COINBASE") {
      subscribe_msg = R"({"type":"subscribe","product_ids":[")" + symbol + R"("],"channels":["level2"]})";
    }
    
    if (!subscribe_msg.empty()) {
      uv_websocket_send(&websocket_, subscribe_msg.c_str(), subscribe_msg.length());
      std::cout << "[EXCHANGE_MANAGER] Subscribed to " << symbol << " on " << exchange_name_ << std::endl;
    }
  }
}

void ExchangeManager::unsubscribe_symbol(const std::string& symbol) {
  subscribed_symbols_.erase(
    std::remove(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol),
    subscribed_symbols_.end()
  );
  
  if (connected_.load()) {
    // Send unsubscription message (exchange-specific)
    std::string unsubscribe_msg;
    if (exchange_name_ == "BINANCE") {
      unsubscribe_msg = R"({"method":"UNSUBSCRIBE","params":[")" + symbol + R"(@depth"],"id":1})";
    } else if (exchange_name_ == "COINBASE") {
      unsubscribe_msg = R"({"type":"unsubscribe","product_ids":[")" + symbol + R"("],"channels":["level2"]})";
    }
    
    if (!unsubscribe_msg.empty()) {
      uv_websocket_send(&websocket_, unsubscribe_msg.c_str(), unsubscribe_msg.length());
      std::cout << "[EXCHANGE_MANAGER] Unsubscribed from " << symbol << " on " << exchange_name_ << std::endl;
    }
  }
}

void ExchangeManager::run_event_loop() {
  connect_websocket();
  uv_run(loop_, UV_RUN_DEFAULT);
}

void ExchangeManager::connect_websocket() {
  uv_websocket_init(loop_, &websocket_);
  
  // Set callbacks
  websocket_.on_connect = on_connect;
  websocket_.on_message = on_message;
  websocket_.on_close = on_close;
  websocket_.on_error = on_error;
  
  // Store this pointer for callbacks
  websocket_.data = this;
  
  // Connect
  uv_tcp_connect(&connect_req_, (uv_tcp_t*)&websocket_, 
                uv_ip4_addr("127.0.0.1", 8080), on_connect);
}

void ExchangeManager::schedule_reconnect() {
  if (reconnect_attempts_ >= max_reconnect_attempts_) {
    std::cerr << "[EXCHANGE_MANAGER] Max reconnection attempts reached for " << exchange_name_ << std::endl;
    return;
  }
  
  reconnect_attempts_++;
  
  uv_timer_init(loop_, &reconnect_timer_);
  uv_timer_start(&reconnect_timer_, [](uv_timer_t* timer) {
    ExchangeManager* manager = static_cast<ExchangeManager*>(timer->data);
    manager->connect_websocket();
  }, reconnect_interval_ms_, 0);
  
  std::cout << "[EXCHANGE_MANAGER] Scheduling reconnect attempt " << reconnect_attempts_ 
            << " for " << exchange_name_ << std::endl;
}

void ExchangeManager::handle_message(const std::string& message) {
  if (message_callback_) {
    message_callback_(message);
  }
}

void ExchangeManager::handle_connection(bool connected) {
  connected_.store(connected);
  
  if (connected) {
    reconnect_attempts_ = 0;
    
    // Resubscribe to all symbols
    for (const auto& symbol : subscribed_symbols_) {
      subscribe_symbol(symbol);
    }
  } else {
    schedule_reconnect();
  }
  
  if (connection_callback_) {
    connection_callback_(connected);
  }
}

// Static libuv callbacks
void ExchangeManager::on_connect(uv_connect_t* req, int status) {
  ExchangeManager* manager = static_cast<ExchangeManager*>(req->handle->data);
  
  if (status < 0) {
    std::cerr << "[EXCHANGE_MANAGER] Connection failed for " << manager->exchange_name_ 
              << ": " << uv_strerror(status) << std::endl;
    manager->handle_connection(false);
    return;
  }
  
  std::cout << "[EXCHANGE_MANAGER] Connected to " << manager->exchange_name_ << std::endl;
  manager->handle_connection(true);
}

void ExchangeManager::on_message(uv_websocket_t* handle, const char* data, size_t len) {
  ExchangeManager* manager = static_cast<ExchangeManager*>(handle->data);
  
  std::string message(data, len);
  manager->handle_message(message);
}

void ExchangeManager::on_close(uv_websocket_t* handle) {
  ExchangeManager* manager = static_cast<ExchangeManager*>(handle->data);
  
  std::cout << "[EXCHANGE_MANAGER] Connection closed for " << manager->exchange_name_ << std::endl;
  manager->handle_connection(false);
}

void ExchangeManager::on_error(uv_websocket_t* handle, int error) {
  ExchangeManager* manager = static_cast<ExchangeManager*>(handle->data);
  
  std::cerr << "[EXCHANGE_MANAGER] Error for " << manager->exchange_name_ 
            << ": " << uv_strerror(error) << std::endl;
  manager->handle_connection(false);
}
