#include "exchange_oms_factory.hpp"
#include "exchange_oms.hpp"
#include "oms.hpp"  // For IExchangeOMS interface
#include "exchanges/binance/binance_oms.hpp"
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

std::shared_ptr<IEnhancedExchangeOMS> ExchangeOMSFactory::create_exchange(const ExchangeConfig& config) {
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
  // Create OMS and cast to base interface
  auto oms = create_exchange(config_copy);
  return std::static_pointer_cast<IExchangeOMS>(oms);
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
  register_exchange_type("MOCK", [](const ExchangeConfig& config) -> std::shared_ptr<IEnhancedExchangeOMS> {
    // Create a simple mock that implements IEnhancedExchangeOMS
    class MockEnhancedOMS : public IEnhancedExchangeOMS {
    public:
      MockEnhancedOMS(const std::string& name, double fill_prob, double reject_prob, int delay_ms)
        : name_(name), fill_probability_(fill_prob), reject_probability_(reject_prob), 
          delay_ms_(delay_ms), connected_(false) {}
      
      Result<bool> connect() override { 
        connected_ = true;
        return Result<bool>(true); 
      }
      
      void disconnect() override { connected_ = false; }
      bool is_connected() const override { return connected_; }
      std::string get_exchange_name() const override { return name_; }
      
      Result<OrderResponse> send_order(const Order& order) override {
        OrderResponse response(order.cl_ord_id, "MOCK_" + std::to_string(std::time(nullptr)), name_, order.symbol);
        response.status = "ACKNOWLEDGED";
        return Result<OrderResponse>(std::move(response));
      }
      
      Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override {
        return Result<bool>(true);
      }
      
      Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                               double new_price, double new_qty) override {
        return Result<bool>(true);
      }
      
      std::vector<std::string> get_supported_symbols() const override {
        return {"BTCUSDT", "ETHUSDT"};
      }
      
      Result<std::map<std::string, std::string>> get_health_status() const override {
        std::map<std::string, std::string> status;
        status["connected"] = connected_ ? "true" : "false";
        status["exchange"] = name_;
        return status;
      }
      
      Result<std::map<std::string, double>> get_performance_metrics() const override {
        std::map<std::string, double> metrics;
        metrics["orders_sent"] = 0;
        return metrics;
      }
      
    private:
      std::string name_;
      double fill_probability_;
      double reject_probability_;
      int delay_ms_;
      bool connected_;
    };
    
    return std::make_shared<MockEnhancedOMS>(
      config.name,
      config.fill_probability,
      config.reject_probability,
      config.response_delay_ms
    );
  });
  
  // Register Binance exchange creator (real implementation)
  register_exchange_type("BINANCE", [](const ExchangeConfig& config) -> std::shared_ptr<IEnhancedExchangeOMS> {
    std::cout << "[EXCHANGE_FACTORY] Creating Binance OMS for " << config.name << std::endl;
    
    binance_real::BinanceConfig binance_config;
    binance_config.api_key = config.api_key;
    binance_config.api_secret = config.api_secret;
    binance_config.exchange_name = config.name;  // Use configurable exchange name
    binance_config.fill_probability = config.fill_probability;
    binance_config.reject_probability = config.reject_probability;
    
    // Set custom parameters if available
    auto it = config.custom_params.find("BASE_URL");
    if (it != config.custom_params.end()) {
      binance_config.base_url = it->second;
    }
    
    it = config.custom_params.find("WS_URL");
    if (it != config.custom_params.end()) {
      binance_config.ws_url = it->second;
    }
    
    it = config.custom_params.find("TIMEOUT_MS");
    if (it != config.custom_params.end()) {
      try { binance_config.timeout_ms = std::stoi(it->second); } catch (...) {}
    }
    
    return std::make_shared<binance_real::BinanceRealOMS>(binance_config);
  });
  
  // Register Deribit exchange creator (placeholder for real implementation)
  register_exchange_type("DERIBIT", [](const ExchangeConfig& config) -> std::shared_ptr<IEnhancedExchangeOMS> {
    // TODO: Implement real DeribitOMS
    std::cout << "[EXCHANGE_FACTORY] Creating Deribit OMS for " << config.name << std::endl;
    
    // For now, return a simple mock that implements IEnhancedExchangeOMS
    class MockDeribitOMS : public IEnhancedExchangeOMS {
    public:
      MockDeribitOMS(const std::string& name) : name_(name) {}
      
      Result<bool> connect() override { return Result<bool>(true); }
      void disconnect() override {}
      bool is_connected() const override { return true; }
      std::string get_exchange_name() const override { return name_; }
      
      Result<OrderResponse> send_order(const Order& order) override {
        OrderResponse response(order.cl_ord_id, "DER_" + std::to_string(std::time(nullptr)), "DERIBIT", order.symbol);
        response.status = "ACKNOWLEDGED";
        return Result<OrderResponse>(std::move(response));
      }
      
      Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override {
        return Result<bool>(true);
      }
      
      Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                               double new_price, double new_qty) override {
        return Result<bool>(true);
      }
      
      std::vector<std::string> get_supported_symbols() const override {
        return {"BTC-PERPETUAL", "ETH-PERPETUAL"};
      }
      
      Result<std::map<std::string, std::string>> get_health_status() const override {
        std::map<std::string, std::string> status;
        status["connected"] = "true";
        status["exchange"] = "DERIBIT";
        return status;
      }
      
      Result<std::map<std::string, double>> get_performance_metrics() const override {
        std::map<std::string, double> metrics;
        metrics["orders_sent"] = 0;
        return metrics;
      }
      
    private:
      std::string name_;
    };
    
    return std::make_shared<MockDeribitOMS>(config.name);
  });
  
  // Register GRVT exchange creator (placeholder for real implementation)
  register_exchange_type("GRVT", [](const ExchangeConfig& config) -> std::shared_ptr<IEnhancedExchangeOMS> {
    // TODO: Implement real GRVT_OMS
    std::cout << "[EXCHANGE_FACTORY] Creating GRVT OMS for " << config.name << std::endl;
    
    // For now, return a simple mock that implements IEnhancedExchangeOMS
    class MockGRVTOMS : public IEnhancedExchangeOMS {
    public:
      MockGRVTOMS(const std::string& name) : name_(name) {}
      
      Result<bool> connect() override { return Result<bool>(true); }
      void disconnect() override {}
      bool is_connected() const override { return true; }
      std::string get_exchange_name() const override { return name_; }
      
      Result<OrderResponse> send_order(const Order& order) override {
        OrderResponse response(order.cl_ord_id, "GRVT_" + std::to_string(std::time(nullptr)), "GRVT", order.symbol);
        response.status = "ACKNOWLEDGED";
        return Result<OrderResponse>(std::move(response));
      }
      
      Result<bool> cancel_order(const std::string& cl_ord_id, const std::string& exchange_order_id) override {
        return Result<bool>(true);
      }
      
      Result<bool> modify_order(const std::string& cl_ord_id, const std::string& exchange_order_id,
                               double new_price, double new_qty) override {
        return Result<bool>(true);
      }
      
      std::vector<std::string> get_supported_symbols() const override {
        return {"BTCUSDC", "ETHUSDC"};
      }
      
      Result<std::map<std::string, std::string>> get_health_status() const override {
        std::map<std::string, std::string> status;
        status["connected"] = "true";
        status["exchange"] = "GRVT";
        return status;
      }
      
      Result<std::map<std::string, double>> get_performance_metrics() const override {
        std::map<std::string, double> metrics;
        metrics["orders_sent"] = 0;
        return metrics;
      }
      
    private:
      std::string name_;
    };
    
    return std::make_shared<MockGRVTOMS>(config.name);
  });
  
  initialized_ = true;
  std::cout << "[EXCHANGE_FACTORY] Initialized with " << creators_.size() << " exchange types" << std::endl;
}
