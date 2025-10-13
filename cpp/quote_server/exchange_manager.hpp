#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <uv.h>

// Single exchange websocket manager
class ExchangeManager {
public:
  using MessageCallback = std::function<void(const std::string& message)>;
  using ConnectionCallback = std::function<void(bool connected)>;
  
  ExchangeManager(const std::string& exchange_name, 
                 const std::string& websocket_url);
  ~ExchangeManager();
  
  // Lifecycle
  bool start();
  void stop();
  bool is_connected() const { return connected_.load(); }
  
  // Symbol management
  void subscribe_symbol(const std::string& symbol);
  void unsubscribe_symbol(const std::string& symbol);
  
  // Callbacks
  void set_message_callback(MessageCallback callback) { message_callback_ = callback; }
  void set_connection_callback(ConnectionCallback callback) { connection_callback_ = callback; }
  
  // Configuration
  void set_reconnect_interval_ms(int ms) { reconnect_interval_ms_ = ms; }
  void set_max_reconnect_attempts(int attempts) { max_reconnect_attempts_ = attempts; }

private:
  // libuv callbacks
  static void on_connect(uv_connect_t* req, int status);
  static void on_message(uv_websocket_t* handle, const char* data, size_t len);
  static void on_close(uv_websocket_t* handle);
  static void on_error(uv_websocket_t* handle, int error);
  
  // Internal methods
  void connect_websocket();
  void schedule_reconnect();
  void handle_message(const std::string& message);
  void handle_connection(bool connected);
  
  // libuv event loop
  void run_event_loop();
  
  std::string exchange_name_;
  std::string websocket_url_;
  
  // libuv handles
  uv_loop_t* loop_;
  uv_websocket_t websocket_;
  uv_connect_t connect_req_;
  uv_timer_t reconnect_timer_;
  
  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread event_loop_thread_;
  
  // Configuration
  int reconnect_interval_ms_{5000}; // 5 seconds
  int max_reconnect_attempts_{10};
  int reconnect_attempts_{0};
  
  // Callbacks
  MessageCallback message_callback_;
  ConnectionCallback connection_callback_;
  
  // Subscribed symbols
  std::vector<std::string> subscribed_symbols_;
};
