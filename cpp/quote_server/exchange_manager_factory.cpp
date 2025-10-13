#include "i_exchange_manager.hpp"
#include "binance_exchange_manager.hpp"
#include "coinbase_exchange_manager.hpp"
#include <iostream>
#include <algorithm>

std::unique_ptr<IExchangeManager> ExchangeManagerFactory::create(const std::string& exchange_name,
                                                               const std::string& websocket_url) {
  std::string exchange_upper = exchange_name;
  std::transform(exchange_upper.begin(), exchange_upper.end(), exchange_upper.begin(), ::toupper);
  
  if (exchange_upper == "BINANCE") {
    return std::make_unique<BinanceExchangeManager>(websocket_url);
  } else if (exchange_upper == "COINBASE" || exchange_upper == "COINBASE_PRO") {
    return std::make_unique<CoinbaseExchangeManager>(websocket_url);
  } else {
    std::cerr << "[FACTORY] Unknown exchange: " << exchange_name << std::endl;
    return nullptr;
  }
}
