#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio> client;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// Exchange-specific websocket client interface
class IExchangeClient {
public:
  virtual ~IExchangeClient() = default;
  virtual bool connect(const std::string& symbol) = 0;
  virtual void disconnect() = 0;
  virtual bool is_connected() const = 0;
};

// Binance websocket client
class BinanceClient : public IExchangeClient {
public:
  BinanceClient();
  ~BinanceClient();
  
  bool connect(const std::string& symbol) override;
  void disconnect() override;
  bool is_connected() const override;
  
  void set_message_callback(std::function<void(const std::string&)> callback) {
    message_callback_ = callback;
  }

private:
  void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
  void run_io_thread();
  
  client c_;
  websocketpp::connection_hdl hdl_;
  std::thread io_thread_;
  std::atomic<bool> connected_{false};
  std::function<void(const std::string&)> message_callback_;
};

// Coinbase websocket client
class CoinbaseClient : public IExchangeClient {
public:
  CoinbaseClient();
  ~CoinbaseClient();
  
  bool connect(const std::string& symbol) override;
  void disconnect() override;
  bool is_connected() const override;
  
  void set_message_callback(std::function<void(const std::string&)> callback) {
    message_callback_ = callback;
  }

private:
  void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
  void run_io_thread();
  
  client c_;
  websocketpp::connection_hdl hdl_;
  std::thread io_thread_;
  std::atomic<bool> connected_{false};
  std::function<void(const std::string&)> message_callback_;
};
