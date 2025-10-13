#pragma once
#include <cstdint>
#include <string>

// Simple binary position format for same-host communication
struct PositionBinary {
  // Header (16 bytes)
  uint64_t timestamp_us;    // microsecond timestamp
  uint32_t symbol_len;      // length of symbol string
  uint32_t exch_len;        // length of exchange string
  
  // Variable length fields
  char symbol[32];          // fixed 32 chars max, null-padded
  char exch[16];            // fixed 16 chars max, null-padded
  
  // Position details
  double qty;               // position quantity (positive=long, negative=short)
  double avg_price;         // average price
  
  // Total size = 16 + 32 + 16 + 8 + 8 = 80 bytes
};

// Helper functions for binary position serialization
class PositionBinaryHelper {
public:
  static constexpr size_t POSITION_SIZE = 80;
  
  static void serialize_position(const std::string& symbol,
                                const std::string& exch,
                                double qty,
                                double avg_price,
                                char* buffer);
                                
  static bool deserialize_position(const char* buffer,
                                  std::string& symbol,
                                  std::string& exch,
                                  double& qty,
                                  double& avg_price,
                                  uint64_t& timestamp_us);
};
