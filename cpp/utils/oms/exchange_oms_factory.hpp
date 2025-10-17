#pragma once
#include <string>
#include <memory>
#include <map>
#include <functional>
#include "../oms/enhanced_oms.hpp"

// Exchange configuration structure
struct ExchangeConfig {
  std::string name;
  std::string type;  // "MOCK", "BINANCE", "DERIBIT", "GRVT"
  std::string api_key;
  std::string api_secret;
  std::string websocket_url;
  double fill_probability{0.8};
  double reject_probability{0.1};
  int response_delay_ms{100};
  bool enabled{true};
  std::map<std::string, std::string> custom_params;
};

// Exchange factory for creating exchange OMS instances
class ExchangeOMSFactory {
public:
  using ExchangeCreator = std::function<std::shared_ptr<IExchangeOMS>(const ExchangeConfig&)>;
  
  // Register exchange type creators
  static void register_exchange_type(const std::string& type, ExchangeCreator creator);
  
  // Create exchange OMS from configuration
  static std::shared_ptr<IExchangeOMS> create_exchange(const ExchangeConfig& config);
  
  // Create exchange OMS from type string
  static std::shared_ptr<IExchangeOMS> create_exchange(const std::string& type, const ExchangeConfig& config);
  
  // Load exchanges from configuration file
  static std::vector<ExchangeConfig> load_exchanges_from_config(const std::string& config_file);
  
  // Get supported exchange types
  static std::vector<std::string> get_supported_types();
  
private:
  static std::map<std::string, ExchangeCreator> creators_;
  static bool initialized_;
  
  static void initialize_default_creators();
};
