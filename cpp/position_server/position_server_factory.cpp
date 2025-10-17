#include "position_server_factory.hpp"
#include "exchanges/binance/binance_position_feed.hpp"
#include "exchanges/deribit/deribit_position_feed.hpp"
#include "../utils/pms/position_feed.hpp"
#include <iostream>
#include <algorithm>

std::map<std::string, PositionServerFactory::ExchangeType> PositionServerFactory::exchange_map_ = {
  {"BINANCE", ExchangeType::BINANCE},
  {"DERIBIT", ExchangeType::DERIBIT},
  {"MOCK", ExchangeType::MOCK}
};

std::unique_ptr<IExchangePositionFeed> PositionServerFactory::create(
  ExchangeType exchange_type,
  const std::string& api_key,
  const std::string& api_secret) {
  
  switch (exchange_type) {
    case ExchangeType::BINANCE:
      if (api_key.empty() || api_secret.empty()) {
        std::cout << "[POSITION_SERVER_FACTORY] Warning: Binance requires API key and secret" << std::endl;
        return std::make_unique<MockPositionFeed>();
      }
      return std::make_unique<BinancePositionFeed>(api_key, api_secret);
      
    case ExchangeType::DERIBIT:
      if (api_key.empty() || api_secret.empty()) {
        std::cout << "[POSITION_SERVER_FACTORY] Warning: Deribit requires client ID and secret" << std::endl;
        return std::make_unique<MockPositionFeed>();
      }
      return std::make_unique<DeribitPositionFeed>(api_key, api_secret);
      
    case ExchangeType::MOCK:
    default:
      return std::make_unique<MockPositionFeed>();
  }
}

std::unique_ptr<IExchangePositionFeed> PositionServerFactory::create_from_string(
  const std::string& exchange_name,
  const std::string& api_key,
  const std::string& api_secret) {
  
  std::string upper_name = exchange_name;
  std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
  
  auto it = exchange_map_.find(upper_name);
  if (it != exchange_map_.end()) {
    return create(it->second, api_key, api_secret);
  }
  
  std::cout << "[POSITION_SERVER_FACTORY] Unknown exchange: " << exchange_name 
            << ", falling back to mock" << std::endl;
  return std::make_unique<MockPositionFeed>();
}
