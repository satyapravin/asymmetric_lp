#pragma once
#include "i_exchange_manager.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <uv.h>

// Binance-specific exchange manager
class BinanceExchangeManager : public IExchangeManager {
public:
  BinanceExchangeManager(const std::string& websocket_url);
  ~BinanceExchangeManager() override;
  
  // IExchangeManager interface
  bool start() override;
  void stop() override;
  bool is_connected() const override { return connected_.load(); }
  
  void subscribe_symbol(const std::string& symbol) override;
  void unsubscribe_symbol(const std::string& symbol) override;
  
  void set_message_callback(MessageCallback callback) override { message_callback_ = callback; }
  void set_connection_callback(ConnectionCallback callback) override { connection_callback_ = callback; }
  
  // Binance-specific methods
  void subscribe_depth_stream(const std::string& symbol);
  void subscribe_trade_stream(const std::string& symbol);
  void subscribe_kline_stream(const std::string& symbol, const std::string& interval);
  
  // Authentication (for private streams)
  void set_api_key(const std::string& key) override { api_key_ = key; }
  void set_secret_key(const std::string& secret) override { secret_key_ = secret; }
  
private:
  // Binance-specific websocket handling
  void connect_to_binance();
  void handle_binance_message(const std::string& message);
  void send_subscription_request(const std::string& stream_name);
  
  // libuv callbacks
  static void on_connect(uv_connect_t* req, int status);
  static void on_message(uv_websocket_t* handle, const char* data, size_t len);
  static void on_close(uv_websocket_t* handle);
  static void on_error(uv_websocket_t* handle, int error);
  
  // Event loop
  void run_event_loop();
  
  std::string websocket_url_;
  std::string api_key_;
  std::string secret_key_;
  
  // libuv handles
  uv_loop_t* loop_;
  uv_websocket_t websocket_;
  uv_connect_t connect_req_;
  uv_timer_t reconnect_timer_;
  
  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread event_loop_thread_;
  
  // Binance-specific state
  std::vector<std::string> subscribed_streams_;
  int request_id_{1}; // For subscription requests
  
  // Callbacks
  MessageCallback message_callback_;
  ConnectionCallback connection_callback_;
};
