#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>
#include <utility>

// Base interface for exchange-specific managers
class IExchangeManager {
public:
  using MessageCallback = std::function<void(const std::string& message)>;
  using ConnectionCallback = std::function<void(bool connected)>;
  
  virtual ~IExchangeManager() = default;
  
  // Lifecycle
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual bool is_connected() const = 0;
  
  // Symbol management
  virtual void subscribe_symbol(const std::string& symbol) = 0;
  virtual void unsubscribe_symbol(const std::string& symbol) = 0;
  
  // Callbacks
  virtual void set_message_callback(MessageCallback callback) = 0;
  virtual void set_connection_callback(ConnectionCallback callback) = 0;

  // Optional: custom key/value configuration from per-exchange section
  virtual void configure_kv(const std::vector<std::pair<std::string, std::string>>& kv) {}
  
  // Exchange-specific configuration
  virtual void set_api_key(const std::string& key) {}
  virtual void set_secret_key(const std::string& secret) {}
  virtual void set_passphrase(const std::string& passphrase) {}
  virtual void set_sandbox_mode(bool enabled) {}
};

// Exchange manager factory
class ExchangeManagerFactory {
public:
  static std::unique_ptr<IExchangeManager> create(const std::string& exchange_name,
                                                  const std::string& websocket_url);
};
