#include "i_exchange_manager.hpp"
#include "exchange_manager.hpp"
#include <iostream>
#include <algorithm>

std::unique_ptr<IExchangeManager> ExchangeManagerFactory::create(const std::string& exchange_name,
                                                               const std::string& websocket_url) {
  std::string exchange_upper = exchange_name;
  std::transform(exchange_upper.begin(), exchange_upper.end(), exchange_upper.begin(), ::toupper);
  
  // For now, just return the base ExchangeManager for all exchanges
  // This can be extended later with exchange-specific implementations
  return std::make_unique<ExchangeManager>(exchange_name, websocket_url);
}
