#include "orderbook_binary.hpp"
#include <cstring>
#include <algorithm>

void OrderBookBinaryHelper::serialize(const std::string& symbol, 
                                     const std::vector<std::pair<double, double>>& bids,
                                     const std::vector<std::pair<double, double>>& asks,
                                     uint64_t timestamp_us,
                                     uint32_t sequence,
                                     char* buffer,
                                     size_t buffer_size) {
  if (!buffer || buffer_size < calculate_size(symbol.length(), bids.size(), asks.size())) {
    return;
  }
  
  OrderBookBinary* ob = reinterpret_cast<OrderBookBinary*>(buffer);
  ob->timestamp_us = timestamp_us;
  ob->sequence = sequence;
  ob->symbol_len = symbol.length();
  
  // Copy symbol (null-padded)
  std::memset(ob->symbol, 0, 32);
  std::strncpy(ob->symbol, symbol.c_str(), std::min(symbol.length(), size_t(31)));
  
  ob->bid_count = bids.size();
  ob->ask_count = asks.size();
  
  // Copy bid levels (highest to lowest)
  double* level_ptr = ob->levels;
  for (const auto& bid : bids) {
    *level_ptr++ = bid.first;   // price
    *level_ptr++ = bid.second;  // size
  }
  
  // Copy ask levels (lowest to highest)
  for (const auto& ask : asks) {
    *level_ptr++ = ask.first;   // price
    *level_ptr++ = ask.second; // size
  }
}

bool OrderBookBinaryHelper::deserialize(const char* buffer, 
                                       size_t buffer_size,
                                       std::string& symbol,
                                       std::vector<std::pair<double, double>>& bids,
                                       std::vector<std::pair<double, double>>& asks,
                                       uint64_t& timestamp_us,
                                       uint32_t& sequence) {
  if (!buffer || buffer_size < 16) return false;
  
  const OrderBookBinary* ob = reinterpret_cast<const OrderBookBinary*>(buffer);
  
  if (buffer_size < calculate_size(ob->symbol_len, ob->bid_count, ob->ask_count)) {
    return false;
  }
  
  timestamp_us = ob->timestamp_us;
  sequence = ob->sequence;
  
  symbol.assign(ob->symbol, ob->symbol_len);
  
  bids.clear();
  asks.clear();
  
  const double* level_ptr = ob->levels;
  
  // Read bid levels
  for (uint32_t i = 0; i < ob->bid_count; ++i) {
    bids.emplace_back(*level_ptr++, *level_ptr++);
  }
  
  // Read ask levels
  for (uint32_t i = 0; i < ob->ask_count; ++i) {
    asks.emplace_back(*level_ptr++, *level_ptr++);
  }
  
  return true;
}
