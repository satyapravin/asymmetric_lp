#include "i_exchange_handler.hpp"
#include "binance/binance_exchange_handler.hpp"
#include <iostream>
#include <algorithm>

std::unique_ptr<IExchangeHandler> ExchangeHandlerFactory::create(const std::string& exchange_name) {
  std::string exchange_upper = exchange_name;
  std::transform(exchange_upper.begin(), exchange_upper.end(), exchange_upper.begin(), ::toupper);
  
  if (exchange_upper == "BINANCE") {
    return std::make_unique<BinanceExchangeHandler>();
  } else if (exchange_upper == "COINBASE" || exchange_upper == "COINBASE_PRO") {
    // return std::make_unique<CoinbaseExchangeHandler>();
    std::cerr << "[FACTORY] Coinbase handler not implemented yet" << std::endl;
    return nullptr;
  } else {
    std::cerr << "[FACTORY] Unknown exchange: " << exchange_name << std::endl;
    return nullptr;
  }
}
