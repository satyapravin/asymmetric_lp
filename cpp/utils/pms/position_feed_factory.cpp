#include "position_feed_factory.hpp"
#include "position_feed.hpp"
#include <iostream>

std::map<std::string, PositionFeedFactory::ExchangeType> PositionFeedFactory::exchange_map_ = {
  {"BINANCE", ExchangeType::BINANCE},
  {"DERIBIT", ExchangeType::DERIBIT},
  {"MOCK", ExchangeType::MOCK}
};

std::unique_ptr<IExchangePositionFeed> PositionFeedFactory::create(
  ExchangeType exchange_type,
  const std::string& api_key,
  const std::string& api_secret) {
  
  switch (exchange_type) {
    case ExchangeType::BINANCE:
      if (api_key.empty() || api_secret.empty()) {
        std::cout << "[POSITION_FACTORY] Warning: Binance requires API key and secret" << std::endl;
        return std::make_unique<MockPositionFeed>();
      }
      // Note: BinancePositionFeed is now in position_server/exchanges/binance/
      // This factory is deprecated - use PositionServerFactory instead
      std::cout << "[POSITION_FACTORY] Warning: BinancePositionFeed moved to position_server. Use PositionServerFactory instead." << std::endl;
      return std::make_unique<MockPositionFeed>();
      
    case ExchangeType::DERIBIT:
      if (api_key.empty() || api_secret.empty()) {
        std::cout << "[POSITION_FACTORY] Warning: Deribit requires client ID and secret" << std::endl;
        return std::make_unique<MockPositionFeed>();
      }
      // Note: DeribitPositionFeed is now in position_server/exchanges/deribit/
      // This factory is deprecated - use PositionServerFactory instead
      std::cout << "[POSITION_FACTORY] Warning: DeribitPositionFeed moved to position_server. Use PositionServerFactory instead." << std::endl;
      return std::make_unique<MockPositionFeed>();
      
    case ExchangeType::MOCK:
    default:
      return std::make_unique<MockPositionFeed>();
  }
}

std::unique_ptr<IExchangePositionFeed> PositionFeedFactory::create_from_string(
  const std::string& exchange_name,
  const std::string& api_key,
  const std::string& api_secret) {
  
  std::string upper_name = exchange_name;
  std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
  
  auto it = exchange_map_.find(upper_name);
  if (it != exchange_map_.end()) {
    return create(it->second, api_key, api_secret);
  }
  
  std::cout << "[POSITION_FACTORY] Unknown exchange: " << exchange_name 
            << ", falling back to mock" << std::endl;
  return std::make_unique<MockPositionFeed>();
}
