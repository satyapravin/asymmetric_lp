#pragma once
#include <string>

enum class Side { Buy, Sell };

enum class OrderEventType { Ack, Fill, Reject, Cancel };

inline const char* to_string(Side s) {
  return s == Side::Buy ? "Buy" : "Sell";
}

inline const char* to_string(OrderEventType t) {
  switch (t) {
    case OrderEventType::Ack: return "Ack";
    case OrderEventType::Fill: return "Fill";
    case OrderEventType::Reject: return "Reject";
    case OrderEventType::Cancel: return "Cancel";
  }
  return "Unknown";
}


