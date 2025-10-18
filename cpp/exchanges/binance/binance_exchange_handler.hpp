#pragma once
#include "i_exchange_handler.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <uv.h>

// Binance-specific exchange handler
class BinanceExchangeHandler : public IExchangeHandler {
public:
  BinanceExchangeHandler();
  ~BinanceExchangeHandler() override;
  
  // IExchangeHandler interface
  bool start() override;
  void stop() override;
  bool is_connected() const override { return connected_.load(); }
  
  bool send_order(const Order& order) override;
  bool cancel_order(const std::string& client_order_id) override;
  bool modify_order(const Order& order) override;
  
  std::vector<Order> get_open_orders() override;
  Order get_order_status(const std::string& client_order_id) override;
  
  void set_order_event_callback(OrderEventCallback callback) override { order_event_callback_ = callback; }
  
  // Binance-specific configuration
  void set_api_key(const std::string& key) override { api_key_ = key; }
  void set_secret_key(const std::string& secret) override { secret_key_ = secret; }
  
  std::string get_exchange_name() const override { return "BINANCE"; }
  
  // Binance-specific methods
  void set_testnet_mode(bool enabled) { testnet_mode_ = enabled; }
  void set_recv_window(int window_ms) { recv_window_ms_ = window_ms; }
  
private:
  // Binance API handling
  void connect_to_binance_api();
  void handle_binance_response(const std::string& response);
  void send_binance_order(const Order& order);
  void cancel_binance_order(const std::string& client_order_id);
  
  // Authentication
  std::string generate_signature(const std::string& query_string);
  std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body);
  
  // HTTP client (using libuv)
  void make_http_request(const std::string& method, const std::string& endpoint, 
                        const std::string& body, const std::string& headers);
  
  // libuv callbacks
  static void on_http_response(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
  static void on_http_connect(uv_connect_t* req, int status);
  
  // Event loop
  void run_event_loop();
  
  std::string api_key_;
  std::string secret_key_;
  bool testnet_mode_{false};
  int recv_window_ms_{5000};
  
  // Binance API endpoints
  std::string base_url_;
  std::string ws_url_;
  
  // libuv handles
  uv_loop_t* loop_;
  uv_tcp_t tcp_client_;
  std::thread event_loop_thread_;
  
  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  
  // Order tracking
  std::map<std::string, Order> active_orders_;
  std::mutex orders_mutex_;
  
  // Callbacks
  OrderEventCallback order_event_callback_;
};
