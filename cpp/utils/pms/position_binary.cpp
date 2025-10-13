#include "position_binary.hpp"
#include <cstring>
#include <algorithm>
#include <chrono>

void PositionBinaryHelper::serialize_position(const std::string& symbol,
                                            const std::string& exch,
                                            double qty,
                                            double avg_price,
                                            char* buffer) {
  if (!buffer) return;
  
  PositionBinary* pos = reinterpret_cast<PositionBinary*>(buffer);
  
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  pos->timestamp_us = now;
  pos->symbol_len = symbol.length();
  pos->exch_len = exch.length();
  
  // Copy strings (null-padded)
  std::memset(pos->symbol, 0, 32);
  std::strncpy(pos->symbol, symbol.c_str(), std::min(symbol.length(), size_t(31)));
  
  std::memset(pos->exch, 0, 16);
  std::strncpy(pos->exch, exch.c_str(), std::min(exch.length(), size_t(15)));
  
  pos->qty = qty;
  pos->avg_price = avg_price;
}

bool PositionBinaryHelper::deserialize_position(const char* buffer,
                                              std::string& symbol,
                                              std::string& exch,
                                              double& qty,
                                              double& avg_price,
                                              uint64_t& timestamp_us) {
  if (!buffer) return false;
  
  const PositionBinary* pos = reinterpret_cast<const PositionBinary*>(buffer);
  
  timestamp_us = pos->timestamp_us;
  symbol.assign(pos->symbol, pos->symbol_len);
  exch.assign(pos->exch, pos->exch_len);
  qty = pos->qty;
  avg_price = pos->avg_price;
  
  return true;
}
