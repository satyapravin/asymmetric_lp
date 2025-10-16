#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include "i_exchange_manager.hpp"

// Single exchange websocket manager (simplified mock version)
class ExchangeManager : public IExchangeManager {
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
  // Internal methods
  void simulate_websocket_connection();
  void handle_message(const std::string& message);
  void handle_connection(bool connected);
  
  // Mock data generation
  void generate_mock_market_data();
  
  std::string exchange_name_;
  std::string websocket_url_;
  
  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread mock_thread_;
  
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