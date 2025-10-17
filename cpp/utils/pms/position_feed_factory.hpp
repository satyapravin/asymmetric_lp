#pragma once
#include <string>
#include <memory>
#include <map>
#include "position_feed.hpp"

// Forward declarations
class BinancePositionFeed;
class DeribitPositionFeed;

// Factory for creating exchange-specific position feeds
class PositionFeedFactory {
public:
  enum class ExchangeType {
    BINANCE,
    DERIBIT,
    MOCK
  };
  
  // Create position feed based on exchange type
  static std::unique_ptr<IExchangePositionFeed> create(
    ExchangeType exchange_type,
    const std::string& api_key = "",
    const std::string& api_secret = ""
  );
  
  // Create position feed from string
  static std::unique_ptr<IExchangePositionFeed> create_from_string(
    const std::string& exchange_name,
    const std::string& api_key = "",
    const std::string& api_secret = ""
  );
  
private:
  static std::map<std::string, ExchangeType> exchange_map_;
};
