#include "i_exchange_manager.hpp"
#include "exchange_manager.hpp"
#include "exchanges/binance/binance_manager.hpp"
#include "exchanges/deribit/deribit_manager.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

std::unique_ptr<IExchangeManager> ExchangeManagerFactory::create(const std::string& exchange_name,
                                                               const std::string& websocket_url) {
  std::string exchange_upper = exchange_name;
  std::transform(exchange_upper.begin(), exchange_upper.end(), exchange_upper.begin(), [](unsigned char c){ return std::toupper(c); });
  
  std::cout << "[FACTORY] Creating exchange manager for: " << exchange_upper << std::endl;
  
  if (exchange_upper == "BINANCE") {
    return std::make_unique<BinanceManager>(websocket_url);
  }
  else if (exchange_upper == "DERIBIT") {
    return std::make_unique<DeribitManager>(websocket_url);
  }
  else if (exchange_upper == "GRVT") {
    // TODO: Implement GRVT manager
    std::cerr << "[FACTORY] GRVT manager not implemented yet" << std::endl;
    return nullptr;
  }
  else {
    std::cerr << "[FACTORY] Unknown exchange: " << exchange_name << ", using generic manager" << std::endl;
    return std::make_unique<ExchangeManager>(exchange_name, websocket_url);
  }
}
