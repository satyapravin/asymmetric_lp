#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include "../../i_exchange_manager.hpp"

class ExchangeManagerBase : public IExchangeManager {
public:
  void set_message_callback(IExchangeManager::MessageCallback cb) override { message_callback_ = std::move(cb); }
  void set_connection_callback(IExchangeManager::ConnectionCallback cb) override { connection_callback_ = std::move(cb); }
  bool is_connected() const override { return connected_.load(); }

protected:
  void emit_message(const std::string& msg) {
    if (message_callback_) message_callback_(msg);
  }
  void emit_connection(bool up) {
    connected_.store(up);
    if (connection_callback_) connection_callback_(up);
  }

  IExchangeManager::MessageCallback message_callback_;
  IExchangeManager::ConnectionCallback connection_callback_;
  std::atomic<bool> connected_{false};
};


