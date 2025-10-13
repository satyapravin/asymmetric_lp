#include "order_binary.hpp"
#include <cstring>
#include <algorithm>
#include <chrono>

void OrderBinaryHelper::serialize_order(const std::string& cl_ord_id,
                                       const std::string& exch,
                                       const std::string& symbol,
                                       uint32_t side,
                                       uint32_t is_market,
                                       double qty,
                                       double price,
                                       char* buffer) {
  if (!buffer) return;
  
  OrderBinary* order = reinterpret_cast<OrderBinary*>(buffer);
  
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  order->timestamp_us = now;
  order->sequence = 0; // Will be set by sender
  order->cl_ord_id_len = cl_ord_id.length();
  order->exch_len = exch.length();
  order->symbol_len = symbol.length();
  order->side = side;
  order->is_market = is_market;
  
  // Copy strings (null-padded)
  std::memset(order->cl_ord_id, 0, 32);
  std::strncpy(order->cl_ord_id, cl_ord_id.c_str(), std::min(cl_ord_id.length(), size_t(31)));
  
  std::memset(order->exch, 0, 16);
  std::strncpy(order->exch, exch.c_str(), std::min(exch.length(), size_t(15)));
  
  std::memset(order->symbol, 0, 32);
  std::strncpy(order->symbol, symbol.c_str(), std::min(symbol.length(), size_t(31)));
  
  order->qty = qty;
  order->price = price;
}

bool OrderBinaryHelper::deserialize_order(const char* buffer,
                                         std::string& cl_ord_id,
                                         std::string& exch,
                                         std::string& symbol,
                                         uint32_t& side,
                                         uint32_t& is_market,
                                         double& qty,
                                         double& price) {
  if (!buffer) return false;
  
  const OrderBinary* order = reinterpret_cast<const OrderBinary*>(buffer);
  
  cl_ord_id.assign(order->cl_ord_id, order->cl_ord_id_len);
  exch.assign(order->exch, order->exch_len);
  symbol.assign(order->symbol, order->symbol_len);
  side = order->side;
  is_market = order->is_market;
  qty = order->qty;
  price = order->price;
  
  return true;
}

void OrderBinaryHelper::serialize_order_event(const std::string& cl_ord_id,
                                             const std::string& exch,
                                             const std::string& symbol,
                                             uint32_t event_type,
                                             double fill_qty,
                                             double fill_price,
                                             const std::string& text,
                                             char* buffer) {
  if (!buffer) return;
  
  OrderEventBinary* event = reinterpret_cast<OrderEventBinary*>(buffer);
  
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  event->timestamp_us = now;
  event->sequence = 0; // Will be set by sender
  event->cl_ord_id_len = cl_ord_id.length();
  event->exch_len = exch.length();
  event->symbol_len = symbol.length();
  event->event_type = event_type;
  event->reserved = 0;
  
  // Copy strings (null-padded)
  std::memset(event->cl_ord_id, 0, 32);
  std::strncpy(event->cl_ord_id, cl_ord_id.c_str(), std::min(cl_ord_id.length(), size_t(31)));
  
  std::memset(event->exch, 0, 16);
  std::strncpy(event->exch, exch.c_str(), std::min(exch.length(), size_t(15)));
  
  std::memset(event->symbol, 0, 32);
  std::strncpy(event->symbol, symbol.c_str(), std::min(symbol.length(), size_t(31)));
  
  event->fill_qty = fill_qty;
  event->fill_price = fill_price;
  event->text_len = text.length();
  
  std::memset(event->text, 0, 64);
  std::strncpy(event->text, text.c_str(), std::min(text.length(), size_t(63)));
}

bool OrderBinaryHelper::deserialize_order_event(const char* buffer,
                                               std::string& cl_ord_id,
                                               std::string& exch,
                                               std::string& symbol,
                                               uint32_t& event_type,
                                               double& fill_qty,
                                               double& fill_price,
                                               std::string& text) {
  if (!buffer) return false;
  
  const OrderEventBinary* event = reinterpret_cast<const OrderEventBinary*>(buffer);
  
  cl_ord_id.assign(event->cl_ord_id, event->cl_ord_id_len);
  exch.assign(event->exch, event->exch_len);
  symbol.assign(event->symbol, event->symbol_len);
  event_type = event->event_type;
  fill_qty = event->fill_qty;
  fill_price = event->fill_price;
  text.assign(event->text, event->text_len);
  
  return true;
}
