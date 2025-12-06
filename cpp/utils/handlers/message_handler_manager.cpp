#include "message_handler_manager.hpp"
#include "../logging/log_helper.hpp"
#include <algorithm>

MessageHandlerManager::MessageHandlerManager() {
  LOG_INFO_COMP("HANDLER_MANAGER", "Created message handler manager");
}

MessageHandlerManager::~MessageHandlerManager() {
  stop_all();
}

void MessageHandlerManager::add_handler(const MessageHandlerConfig& config) {
  if (!config.enabled) {
    LOG_INFO_COMP("HANDLER_MANAGER", "Skipping disabled handler '" + config.name + "'");
    return;
  }
  
  auto handler = std::make_unique<MessageHandler>(config.name, config.endpoint, config.topic);
  handler->set_data_callback(data_callback_);
  
  handlers_[config.name] = std::move(handler);
  LOG_INFO_COMP("HANDLER_MANAGER", "Added handler '" + config.name + 
                "' for topic '" + config.topic + "'");
}

void MessageHandlerManager::remove_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->stop();
    handlers_.erase(it);
    LOG_INFO_COMP("HANDLER_MANAGER", "Removed handler '" + name + "'");
  }
}

void MessageHandlerManager::clear_handlers() {
  stop_all();
  handlers_.clear();
  LOG_INFO_COMP("HANDLER_MANAGER", "Cleared all handlers");
}

void MessageHandlerManager::start_all() {
  for (auto& [name, handler] : handlers_) {
    handler->start();
  }
  LOG_INFO_COMP("HANDLER_MANAGER", "Started " + std::to_string(handlers_.size()) + " handlers");
}

void MessageHandlerManager::stop_all() {
  for (auto& [name, handler] : handlers_) {
    handler->stop();
  }
  LOG_INFO_COMP("HANDLER_MANAGER", "Stopped all handlers");
}

void MessageHandlerManager::start_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->start();
    LOG_INFO_COMP("HANDLER_MANAGER", "Started handler '" + name + "'");
  }
}

void MessageHandlerManager::stop_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->stop();
    LOG_INFO_COMP("HANDLER_MANAGER", "Stopped handler '" + name + "'");
  }
}

void MessageHandlerManager::set_data_callback(DataCallback callback) {
  data_callback_ = callback;
  
  // Update callback for all existing handlers
  for (auto& [name, handler] : handlers_) {
    handler->set_data_callback(data_callback_);
  }
}

std::vector<std::string> MessageHandlerManager::get_handler_names() const {
  std::vector<std::string> names;
  names.reserve(handlers_.size());
  
  for (const auto& [name, handler] : handlers_) {
    names.push_back(name);
  }
  
  return names;
}

bool MessageHandlerManager::is_handler_running(const std::string& name) const {
  auto it = handlers_.find(name);
  return it != handlers_.end() && it->second->is_running();
}

void MessageHandlerManager::load_from_config(const std::vector<MessageHandlerConfig>& configs) {
  clear_handlers();
  
  for (const auto& config : configs) {
    add_handler(config);
  }
  
  LOG_INFO_COMP("HANDLER_MANAGER", "Loaded " + std::to_string(handlers_.size()) + " handlers from config");
}
