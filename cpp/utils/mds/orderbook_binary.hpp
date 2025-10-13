#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Fast binary orderbook format for same-host communication
// All fields are little-endian for x86_64 compatibility
struct OrderBookBinary {
  // Header (16 bytes)
  uint64_t timestamp_us;    // microsecond timestamp
  uint32_t sequence;        // sequence number for gap detection
  uint32_t symbol_len;      // length of symbol string
  
  // Symbol (variable length, null-terminated)
  char symbol[32];          // fixed 32 chars max, null-padded
  
  // Price levels (variable count)
  uint32_t bid_count;       // number of bid levels
  uint32_t ask_count;       // number of ask levels
  
  // Level data: [price(8), size(8)] pairs
  // Bids first (highest to lowest), then asks (lowest to highest)
  double levels[];           // flexible array: 2 * (bid_count + ask_count) doubles
  
  // Total size = 16 + symbol_len + 4 + 4 + 8 * 2 * (bid_count + ask_count)
};

// Helper functions for binary orderbook
class OrderBookBinaryHelper {
public:
  static size_t calculate_size(uint32_t symbol_len, uint32_t bid_count, uint32_t ask_count) {
    return 16 + symbol_len + 4 + 4 + 8 * 2 * (bid_count + ask_count);
  }
  
  static void serialize(const std::string& symbol, 
                       const std::vector<std::pair<double, double>>& bids,
                       const std::vector<std::pair<double, double>>& asks,
                       uint64_t timestamp_us,
                       uint32_t sequence,
                       char* buffer,
                       size_t buffer_size);
                       
  static bool deserialize(const char* buffer, 
                         size_t buffer_size,
                         std::string& symbol,
                         std::vector<std::pair<double, double>>& bids,
                         std::vector<std::pair<double, double>>& asks,
                         uint64_t& timestamp_us,
                         uint32_t& sequence);
};
