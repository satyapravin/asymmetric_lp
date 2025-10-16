#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "message_handler.hpp"
#include "../config/config.hpp"

// Manager for multiple message handlers
class MessageHandlerManager {
public:
  using DataCallback = std::function<void(const std::string& handler_name, const std::string& data)>;
  
  MessageHandlerManager();
  ~MessageHandlerManager();
  
  // Configuration
  void add_handler(const MessageHandlerConfig& config);
  void remove_handler(const std::string& name);
  void clear_handlers();
  
  // Lifecycle
  void start_all();
  void stop_all();
  void start_handler(const std::string& name);
  void stop_handler(const std::string& name);
  
  // Callbacks
  void set_data_callback(DataCallback callback);
  
  // Status
  std::vector<std::string> get_handler_names() const;
  bool is_handler_running(const std::string& name) const;
  size_t get_handler_count() const { return handlers_.size(); }
  
  // Configuration loading
  void load_from_config(const std::vector<MessageHandlerConfig>& configs);

private:
  std::unordered_map<std::string, std::unique_ptr<IMessageHandler>> handlers_;
  DataCallback data_callback_;
};
