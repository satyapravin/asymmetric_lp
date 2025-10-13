#pragma once
#include "i_exchange_manager.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <uv.h>

// Coinbase Pro-specific exchange manager
class CoinbaseExchangeManager : public IExchangeManager {
public:
  CoinbaseExchangeManager(const std::string& websocket_url);
  ~CoinbaseExchangeManager() override;
  
  // IExchangeManager interface
  bool start() override;
  void stop() override;
  bool is_connected() const override { return connected_.load(); }
  
  void subscribe_symbol(const std::string& symbol) override;
  void unsubscribe_symbol(const std::string& symbol) override;
  
  void set_message_callback(MessageCallback callback) override { message_callback_ = callback; }
  void set_connection_callback(ConnectionCallback callback) override { connection_callback_ = callback; }
  
  // Coinbase-specific methods
  void subscribe_level2(const std::string& symbol);
  void subscribe_matches(const std::string& symbol);
  void subscribe_ticker(const std::string& symbol);
  
  // Authentication
  void set_api_key(const std::string& key) override { api_key_ = key; }
  void set_secret_key(const std::string& secret) override { secret_key_ = secret; }
  void set_passphrase(const std::string& passphrase) override { passphrase_ = passphrase; }
  void set_sandbox_mode(bool enabled) override { sandbox_mode_ = enabled; }
  
private:
  // Coinbase-specific websocket handling
  void connect_to_coinbase();
  void handle_coinbase_message(const std::string& message);
  void send_subscription_request(const std::string& type, const std::string& symbol, const std::string& channel);
  
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
  std::string passphrase_;
  bool sandbox_mode_{false};
  
  // libuv handles
  uv_loop_t* loop_;
  uv_websocket_t websocket_;
  uv_connect_t connect_req_;
  uv_timer_t reconnect_timer_;
  
  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread event_loop_thread_;
  
  // Coinbase-specific state
  std::vector<std::string> subscribed_products_;
  
  // Callbacks
  MessageCallback message_callback_;
  ConnectionCallback connection_callback_;
};
