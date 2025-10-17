#include "exchange_oms_factory.hpp"
#include "mock_exchange_oms.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

// Static member definitions
std::map<std::string, ExchangeOMSFactory::ExchangeCreator> ExchangeOMSFactory::creators_;
bool ExchangeOMSFactory::initialized_ = false;

void ExchangeOMSFactory::register_exchange_type(const std::string& type, ExchangeCreator creator) {
  creators_[type] = creator;
}

std::shared_ptr<IExchangeOMS> ExchangeOMSFactory::create_exchange(const ExchangeConfig& config) {
  if (!initialized_) {
    initialize_default_creators();
  }
  
  auto it = creators_.find(config.type);
  if (it == creators_.end()) {
    std::cerr << "[EXCHANGE_FACTORY] Unsupported exchange type: " << config.type << std::endl;
    return nullptr;
  }
  
  try {
    return it->second(config);
  } catch (const std::exception& e) {
    std::cerr << "[EXCHANGE_FACTORY] Error creating exchange " << config.name 
              << ": " << e.what() << std::endl;
    return nullptr;
  }
}

std::shared_ptr<IExchangeOMS> ExchangeOMSFactory::create_exchange(const std::string& type, const ExchangeConfig& config) {
  ExchangeConfig config_copy = config;
  config_copy.type = type;
  return create_exchange(config_copy);
}

std::vector<ExchangeConfig> ExchangeOMSFactory::load_exchanges_from_config(const std::string& config_file) {
  std::vector<ExchangeConfig> exchanges;
  
  try {
    AppConfig app_config;
    load_from_ini(config_file, app_config);
    
    // Parse exchange configurations from INI sections
    for (const auto& section : app_config.sections) {
      if (section.name.empty()) continue;
      
      ExchangeConfig exchange_config;
      exchange_config.name = section.name;
      
      // Parse section key-value pairs
      for (const auto& [key, value] : section.entries) {
        if (key == "TYPE") exchange_config.type = value;
        else if (key == "API_KEY") exchange_config.api_key = value;
        else if (key == "API_SECRET") exchange_config.api_secret = value;
        else if (key == "WEBSOCKET_URL") exchange_config.websocket_url = value;
        else if (key == "FILL_PROBABILITY") {
          try { exchange_config.fill_probability = std::stod(value); } catch (...) {}
        }
        else if (key == "REJECT_PROBABILITY") {
          try { exchange_config.reject_probability = std::stod(value); } catch (...) {}
        }
        else if (key == "RESPONSE_DELAY_MS") {
          try { exchange_config.response_delay_ms = std::stoi(value); } catch (...) {}
        }
        else if (key == "ENABLED") {
          exchange_config.enabled = (value == "true" || value == "1");
        }
        else {
          // Store custom parameters
          exchange_config.custom_params[key] = value;
        }
      }
      
      if (exchange_config.enabled && !exchange_config.type.empty()) {
        exchanges.push_back(exchange_config);
        std::cout << "[EXCHANGE_FACTORY] Loaded exchange: " << exchange_config.name 
                  << " (type: " << exchange_config.type << ")" << std::endl;
      }
    }
    
  } catch (const std::exception& e) {
    std::cerr << "[EXCHANGE_FACTORY] Error loading config: " << e.what() << std::endl;
  }
  
  return exchanges;
}

std::vector<std::string> ExchangeOMSFactory::get_supported_types() {
  if (!initialized_) {
    initialize_default_creators();
  }
  
  std::vector<std::string> types;
  for (const auto& [type, creator] : creators_) {
    types.push_back(type);
  }
  return types;
}

void ExchangeOMSFactory::initialize_default_creators() {
  if (initialized_) return;
  
  // Register mock exchange creator
  register_exchange_type("MOCK", [](const ExchangeConfig& config) -> std::shared_ptr<IExchangeOMS> {
    return std::make_shared<MockExchangeOMS>(
      config.name,
      config.fill_probability,
      config.reject_probability,
      std::chrono::milliseconds(config.response_delay_ms)
    );
  });
  
  // Register Binance exchange creator (placeholder for real implementation)
  register_exchange_type("BINANCE", [](const ExchangeConfig& config) -> std::shared_ptr<IExchangeOMS> {
    // TODO: Implement real BinanceOMS
    std::cout << "[EXCHANGE_FACTORY] Creating Binance OMS for " << config.name << std::endl;
    return std::make_shared<MockExchangeOMS>(
      config.name,
      config.fill_probability,
      config.reject_probability,
      std::chrono::milliseconds(config.response_delay_ms)
    );
  });
  
  // Register Deribit exchange creator (placeholder for real implementation)
  register_exchange_type("DERIBIT", [](const ExchangeConfig& config) -> std::shared_ptr<IExchangeOMS> {
    // TODO: Implement real DeribitOMS
    std::cout << "[EXCHANGE_FACTORY] Creating Deribit OMS for " << config.name << std::endl;
    return std::make_shared<MockExchangeOMS>(
      config.name,
      config.fill_probability,
      config.reject_probability,
      std::chrono::milliseconds(config.response_delay_ms)
    );
  });
  
  // Register GRVT exchange creator (placeholder for real implementation)
  register_exchange_type("GRVT", [](const ExchangeConfig& config) -> std::shared_ptr<IExchangeOMS> {
    // TODO: Implement real GRVT_OMS
    std::cout << "[EXCHANGE_FACTORY] Creating GRVT OMS for " << config.name << std::endl;
    return std::make_shared<MockExchangeOMS>(
      config.name,
      config.fill_probability,
      config.reject_probability,
      std::chrono::milliseconds(config.response_delay_ms)
    );
  });
  
  initialized_ = true;
  std::cout << "[EXCHANGE_FACTORY] Initialized with " << creators_.size() << " exchange types" << std::endl;
}
