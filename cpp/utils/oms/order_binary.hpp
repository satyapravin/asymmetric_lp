#pragma once
#include <cstdint>
#include <string>

// Fast binary order format for same-host communication
struct OrderBinary {
  // Header (32 bytes)
  uint64_t timestamp_us;    // microsecond timestamp
  uint32_t sequence;        // sequence number
  uint32_t cl_ord_id_len;   // length of client order ID
  uint32_t exch_len;        // length of exchange name
  uint32_t symbol_len;      // length of symbol
  uint32_t side;            // 0=Buy, 1=Sell
  uint32_t is_market;       // 0=Limit, 1=Market
  
  // Variable length fields
  char cl_ord_id[32];       // fixed 32 chars max, null-padded
  char exch[16];            // fixed 16 chars max, null-padded  
  char symbol[32];          // fixed 32 chars max, null-padded
  
  // Order details
  double qty;               // order quantity
  double price;             // order price (0 for market orders)
  
  // Total size = 32 + 32 + 16 + 32 + 8 + 8 = 128 bytes
};

// Fast binary order event format
struct OrderEventBinary {
  // Header (32 bytes)
  uint64_t timestamp_us;    // microsecond timestamp
  uint32_t sequence;        // sequence number
  uint32_t cl_ord_id_len;   // length of client order ID
  uint32_t exch_len;        // length of exchange name
  uint32_t symbol_len;      // length of symbol
  uint32_t event_type;      // 0=Ack, 1=Fill, 2=Reject, 3=Cancel
  uint32_t reserved;         // padding
  
  // Variable length fields
  char cl_ord_id[32];       // fixed 32 chars max, null-padded
  char exch[16];            // fixed 16 chars max, null-padded
  char symbol[32];          // fixed 32 chars max, null-padded
  
  // Event details
  double fill_qty;          // filled quantity
  double fill_price;         // fill price
  uint32_t text_len;        // length of text field
  char text[64];            // fixed 64 chars max, null-padded (reject reason, etc.)
  
  // Total size = 32 + 32 + 16 + 32 + 8 + 8 + 4 + 64 = 196 bytes
};

// Helper functions for binary order serialization
class OrderBinaryHelper {
public:
  static constexpr size_t ORDER_SIZE = 128;
  static constexpr size_t ORDER_EVENT_SIZE = 196;
  
  static void serialize_order(const std::string& cl_ord_id,
                            const std::string& exch,
                            const std::string& symbol,
                            uint32_t side,
                            uint32_t is_market,
                            double qty,
                            double price,
                            char* buffer);
                            
  static bool deserialize_order(const char* buffer,
                               std::string& cl_ord_id,
                               std::string& exch,
                               std::string& symbol,
                               uint32_t& side,
                               uint32_t& is_market,
                               double& qty,
                               double& price);
                               
  static void serialize_order_event(const std::string& cl_ord_id,
                                   const std::string& exch,
                                   const std::string& symbol,
                                   uint32_t event_type,
                                   double fill_qty,
                                   double fill_price,
                                   const std::string& text,
                                   char* buffer);
                                   
  static bool deserialize_order_event(const char* buffer,
                                     std::string& cl_ord_id,
                                     std::string& exch,
                                     std::string& symbol,
                                     uint32_t& event_type,
                                     double& fill_qty,
                                     double& fill_price,
                                     std::string& text);
};
