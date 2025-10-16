#include "message_handler_manager.hpp"
#include <iostream>
#include <algorithm>

MessageHandlerManager::MessageHandlerManager() {
  std::cout << "[HANDLER_MANAGER] Created message handler manager" << std::endl;
}

MessageHandlerManager::~MessageHandlerManager() {
  stop_all();
}

void MessageHandlerManager::add_handler(const MessageHandlerConfig& config) {
  if (!config.enabled) {
    std::cout << "[HANDLER_MANAGER] Skipping disabled handler '" << config.name << "'" << std::endl;
    return;
  }
  
  auto handler = std::make_unique<MessageHandler>(config.name, config.endpoint, config.topic);
  handler->set_data_callback(data_callback_);
  
  handlers_[config.name] = std::move(handler);
  std::cout << "[HANDLER_MANAGER] Added handler '" << config.name 
            << "' for topic '" << config.topic << "'" << std::endl;
}

void MessageHandlerManager::remove_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->stop();
    handlers_.erase(it);
    std::cout << "[HANDLER_MANAGER] Removed handler '" << name << "'" << std::endl;
  }
}

void MessageHandlerManager::clear_handlers() {
  stop_all();
  handlers_.clear();
  std::cout << "[HANDLER_MANAGER] Cleared all handlers" << std::endl;
}

void MessageHandlerManager::start_all() {
  for (auto& [name, handler] : handlers_) {
    handler->start();
  }
  std::cout << "[HANDLER_MANAGER] Started " << handlers_.size() << " handlers" << std::endl;
}

void MessageHandlerManager::stop_all() {
  for (auto& [name, handler] : handlers_) {
    handler->stop();
  }
  std::cout << "[HANDLER_MANAGER] Stopped all handlers" << std::endl;
}

void MessageHandlerManager::start_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->start();
    std::cout << "[HANDLER_MANAGER] Started handler '" << name << "'" << std::endl;
  }
}

void MessageHandlerManager::stop_handler(const std::string& name) {
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    it->second->stop();
    std::cout << "[HANDLER_MANAGER] Stopped handler '" << name << "'" << std::endl;
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
  
  std::cout << "[HANDLER_MANAGER] Loaded " << handlers_.size() << " handlers from config" << std::endl;
}
