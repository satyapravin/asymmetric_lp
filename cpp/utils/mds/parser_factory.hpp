#pragma once
#include <memory>
#include <string>
#include "market_data_normalizer.hpp"

// Factory to create exchange parsers by name
std::unique_ptr<IExchangeParser> create_exchange_parser(const std::string& parser_name,
                                                        const std::string& symbol_for_mock);


