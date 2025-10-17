#include "parser_factory.hpp"
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
  if (name == "MOCK") {
    return std::make_unique<MockParser>(symbol_for_mock, 2000.0);
  }
  // Default to exchange name try; else mock
  if (parser_name == "") {
    return std::make_unique<MockParser>(symbol_for_mock, 2000.0);
  }
  return std::make_unique<MockParser>(symbol_for_mock, 2000.0);
}


