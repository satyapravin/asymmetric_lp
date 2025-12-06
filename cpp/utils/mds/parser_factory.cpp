#include "parser_factory.hpp"
#include "../logging/log_helper.hpp"
#include <algorithm>

std::unique_ptr<IExchangeParser> create_exchange_parser(const std::string& parser_name,
                                                        const std::string& symbol_for_mock) {
  std::string name = parser_name;
  std::transform(name.begin(), name.end(), name.begin(), ::toupper);
  if (name == "BINANCE") {
    return std::make_unique<BinanceParser>();
  }
  if (name == "COINBASE") {
    return std::make_unique<CoinbaseParser>();
  }
  
  // For unsupported exchanges, return nullptr instead of mock
  LOG_ERROR_COMP("PARSER_FACTORY", "Unsupported exchange parser: " + parser_name);
  return nullptr;
}


