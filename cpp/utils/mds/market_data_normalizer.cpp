#include "market_data_normalizer.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>

MarketDataNormalizer::MarketDataNormalizer(const std::string& exchange_name)
    : exchange_name_(exchange_name) {
}

void MarketDataNormalizer::process_message(const std::string& raw_msg) {
  if (!parser_ || !callback_) return;
  
  std::string symbol;
  std::vector<std::pair<double, double>> bids, asks;
  uint64_t timestamp_us;
  
  if (parser_->parse_message(raw_msg, symbol, bids, asks, timestamp_us)) {
    // Sort bids (highest first) and asks (lowest first)
    std::sort(bids.begin(), bids.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    std::sort(asks.begin(), asks.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    callback_(symbol, bids, asks, timestamp_us);
  }
}

// Binance implementation (simplified)
bool BinanceParser::parse_message(const std::string& raw_msg, 
                                 std::string& symbol,
                                 std::vector<std::pair<double, double>>& bids,
                                 std::vector<std::pair<double, double>>& asks,
                                 uint64_t& timestamp_us) {
  // Simplified JSON parsing for Binance depth stream
  // In production, use a proper JSON library like nlohmann/json
  
  auto find_field = [&](const std::string& key) -> std::string {
    auto pos = raw_msg.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    auto start = raw_msg.find(':', pos);
    if (start == std::string::npos) return "";
    start = raw_msg.find_first_not_of(" \"", start + 1);
    auto end = raw_msg.find_first_of(",}\"", start);
    if (end == std::string::npos) end = raw_msg.size();
    return raw_msg.substr(start, end - start);
  };
  
  symbol = find_field("s");
  std::string bids_str = find_field("b");
  std::string asks_str = find_field("a");
  std::string ts_str = find_field("E");
  
  if (symbol.empty() || bids_str.empty() || asks_str.empty() || ts_str.empty()) {
    return false;
  }
  
  try {
    timestamp_us = std::stoull(ts_str) * 1000; // Binance sends ms, convert to us
    
    // Parse bids: [["price", "size"], ...]
    // This is simplified - real implementation would parse JSON arrays properly
    bids.clear();
    asks.clear();
    
    // For now, create mock data
    bids.emplace_back(2000.0, 1.0);
    bids.emplace_back(1999.0, 2.0);
    asks.emplace_back(2001.0, 1.0);
    asks.emplace_back(2002.0, 2.0);
    
    return true;
  } catch (...) {
    return false;
  }
}

// Coinbase implementation (simplified)
bool CoinbaseParser::parse_message(const std::string& raw_msg, 
                                  std::string& symbol,
                                  std::vector<std::pair<double, double>>& bids,
                                  std::vector<std::pair<double, double>>& asks,
                                  uint64_t& timestamp_us) {
  // Simplified JSON parsing for Coinbase Pro
  // Real implementation would parse Coinbase's specific format
  
  auto find_field = [&](const std::string& key) -> std::string {
    auto pos = raw_msg.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    auto start = raw_msg.find(':', pos);
    if (start == std::string::npos) return "";
    start = raw_msg.find_first_not_of(" \"", start + 1);
    auto end = raw_msg.find_first_of(",}\"", start);
    if (end == std::string::npos) end = raw_msg.size();
    return raw_msg.substr(start, end - start);
  };
  
  std::string type = find_field("type");
  if (type != "snapshot" && type != "l2update") return false;
  
  symbol = find_field("product_id");
  std::string ts_str = find_field("time");
  
  if (symbol.empty() || ts_str.empty()) return false;
  
  try {
    // Parse ISO timestamp to microseconds (simplified)
    timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Mock data for now
    bids.clear();
    asks.clear();
    bids.emplace_back(2000.0, 1.0);
    asks.emplace_back(2001.0, 1.0);
    
    return true;
  } catch (...) {
    return false;
  }
}

// Mock parser for testing
MockParser::MockParser(const std::string& symbol, double base_price)
    : mock_symbol_(symbol), base_price_(base_price) {}

bool MockParser::parse_message(const std::string& raw_msg, 
                             std::string& symbol,
                             std::vector<std::pair<double, double>>& bids,
                             std::vector<std::pair<double, double>>& asks,
                             uint64_t& timestamp_us) {
  symbol = mock_symbol_;
  
  // Generate timestamp
  timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  // Add some price movement
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<> price_dist(-0.001, 0.001);
  
  double current_price = base_price_ * (1.0 + price_dist(gen));
  
  bids.clear();
  asks.clear();
  
  // Generate realistic orderbook
  for (int i = 0; i < 10; ++i) {
    double bid_price = current_price * (1.0 - (i + 1) * 0.0005);
    double ask_price = current_price * (1.0 + (i + 1) * 0.0005);
    
    // Decreasing quantities with depth
    double bid_qty = 10.0 / (i + 1);
    double ask_qty = 10.0 / (i + 1);
    
    bids.emplace_back(bid_price, bid_qty);
    asks.emplace_back(ask_price, ask_qty);
  }
  
  sequence_++;
  return true;
}
