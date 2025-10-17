#pragma once
#include <string>
#include <cstdint>
#include "types.hpp"

struct Order {
  std::string cl_ord_id;
  std::string exch;           // routing key, e.g., "GRVT"
  std::string symbol;         // instrument, e.g., "ETHUSDC-PERP"
  Side side{Side::Buy};
  double qty{0.0};            // units
  double price{0.0};          // optional for limit; 0 for mkt
  bool is_market{true};
};

struct OrderEvent {
  std::string cl_ord_id;
  std::string exch;
  std::string symbol;
  OrderEventType type{OrderEventType::Ack};
  double fill_qty{0.0};
  double fill_price{0.0};
  std::string text;           // reject reason, etc.
  std::string exchange_order_id; // Exchange's internal order ID
  uint64_t timestamp_us{0};   // Timestamp in microseconds
};


