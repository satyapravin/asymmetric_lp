#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

// Exchange-specific websocket message parser interface
class IExchangeParser {
public:
  virtual ~IExchangeParser() = default;
  virtual bool parse_message(const std::string& raw_msg, 
                           std::string& symbol,
                           std::vector<std::pair<double, double>>& bids,
                           std::vector<std::pair<double, double>>& asks,
                           uint64_t& timestamp_us) = 0;
};

// Binance parser example
class BinanceParser : public IExchangeParser {
public:
  bool parse_message(const std::string& raw_msg, 
                    std::string& symbol,
                    std::vector<std::pair<double, double>>& bids,
                    std::vector<std::pair<double, double>>& asks,
                    uint64_t& timestamp_us) override;
};

// Coinbase parser example  
class CoinbaseParser : public IExchangeParser {
public:
  bool parse_message(const std::string& raw_msg, 
                    std::string& symbol,
                    std::vector<std::pair<double, double>>& bids,
                    std::vector<std::pair<double, double>>& asks,
                    uint64_t& timestamp_us) override;
};

// Market data normalizer
class MarketDataNormalizer {
public:
  using OrderBookCallback = std::function<void(const std::string& symbol,
                                             const std::vector<std::pair<double, double>>& bids,
                                             const std::vector<std::pair<double, double>>& asks,
                                             uint64_t timestamp_us)>;
  
  MarketDataNormalizer(const std::string& exchange_name);
  
  void set_parser(std::unique_ptr<IExchangeParser> parser) {
    parser_ = std::move(parser);
  }
  
  void set_callback(OrderBookCallback callback) {
    callback_ = callback;
  }
  
  // Process raw websocket message and emit normalized orderbook
  void process_message(const std::string& raw_msg);
  
  // Get next sequence number for gap detection
  uint32_t get_next_sequence() { return ++sequence_; }

private:
  std::string exchange_name_;
  std::unique_ptr<IExchangeParser> parser_;
  OrderBookCallback callback_;
  std::atomic<uint32_t> sequence_{0};
};
