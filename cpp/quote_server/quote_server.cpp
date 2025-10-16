#include "quote_server.hpp"
#include "i_exchange_manager.hpp"
#ifdef PROTO_ENABLED
#include "market_data.pb.h"
#endif
#include <iostream>
#include <algorithm>
#include <chrono>

QuoteServer::QuoteServer(const std::string& exchange_name, const std::string& zmq_endpoint)
    : exchange_name_(exchange_name), zmq_endpoint_(zmq_endpoint) {
  publisher_ = std::make_unique<ZmqPublisher>(zmq_endpoint);
  normalizer_ = std::make_unique<MarketDataNormalizer>(exchange_name);
  
  // Set up normalizer callback
  normalizer_->set_callback([this](const std::string& symbol,
                                 const std::vector<std::pair<double, double>>& bids,
                                 const std::vector<std::pair<double, double>>& asks,
                                 uint64_t timestamp_us) {
    this->process_orderbook_update(symbol, bids, asks, timestamp_us);
  });
  
  std::cout << "[QUOTE_SERVER] Initialized for exchange: " << exchange_name 
            << " publishing on: " << zmq_endpoint << std::endl;
}

QuoteServer::~QuoteServer() {
  stop();
}

void QuoteServer::start() {
  if (running_.load()) return;
  
  running_.store(true);
  publish_thread_ = std::thread([this]() { this->publish_loop(); });
  
  std::cout << "[QUOTE_SERVER] Started publishing at " << publish_rate_hz_ << " Hz" << std::endl;
}

void QuoteServer::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  if (publish_thread_.joinable()) {
    publish_thread_.join();
  }
  
  // Stop exchange manager
  if (exchange_manager_) {
    exchange_manager_->stop();
  }
  
  std::cout << "[QUOTE_SERVER] Stopped" << std::endl;
}

void QuoteServer::set_websocket_url(const std::string& url) {
  websocket_url_ = url;
}

void QuoteServer::connect_to_exchange() {
  if (!websocket_url_.empty()) {
    exchange_manager_ = ExchangeManagerFactory::create(exchange_name_, websocket_url_);
    
    if (exchange_manager_) {
      // Set up exchange manager callbacks
      exchange_manager_->set_message_callback([this](const std::string& message) {
        this->process_raw_message(message);
      });
      
      exchange_manager_->set_connection_callback([this](bool connected) {
        std::cout << "[QUOTE_SERVER] Exchange " << exchange_name_ 
                  << " connection: " << (connected ? "UP" : "DOWN") << std::endl;
      });
      
      exchange_manager_->start();
      std::cout << "[QUOTE_SERVER] Connected to exchange: " << exchange_name_ << std::endl;
    } else {
      std::cerr << "[QUOTE_SERVER] Failed to create exchange manager for " << exchange_name_ << std::endl;
    }
  }
}

void QuoteServer::disconnect_from_exchange() {
  if (exchange_manager_) {
    exchange_manager_->stop();
    exchange_manager_.reset();
    std::cout << "[QUOTE_SERVER] Disconnected from exchange: " << exchange_name_ << std::endl;
  }
}

void QuoteServer::add_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(symbols_mutex_);
  active_symbols_.insert(symbol);
  
  // Subscribe via exchange manager if connected
  if (exchange_manager_ && exchange_manager_->is_connected()) {
    exchange_manager_->subscribe_symbol(symbol);
  }
  
  std::cout << "[QUOTE_SERVER] Added symbol: " << symbol << std::endl;
}

void QuoteServer::remove_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(symbols_mutex_);
  active_symbols_.erase(symbol);
  
  // Unsubscribe via exchange manager if connected
  if (exchange_manager_ && exchange_manager_->is_connected()) {
    exchange_manager_->unsubscribe_symbol(symbol);
  }
  
  std::cout << "[QUOTE_SERVER] Removed symbol: " << symbol << std::endl;
}

std::vector<std::string> QuoteServer::get_active_symbols() const {
  std::lock_guard<std::mutex> lock(symbols_mutex_);
  return std::vector<std::string>(active_symbols_.begin(), active_symbols_.end());
}

void QuoteServer::process_raw_message(const std::string& raw_msg) {
  if (!running_.load()) return;
  
  try {
    normalizer_->process_message(raw_msg);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_processed++;
  } catch (const std::exception& e) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.parse_errors++;
    std::cerr << "[QUOTE_SERVER] Parse error: " << e.what() << std::endl;
  }
}

void QuoteServer::process_orderbook_update(const std::string& symbol,
                                         const std::vector<std::pair<double, double>>& bids,
                                         const std::vector<std::pair<double, double>>& asks,
                                         uint64_t timestamp_us) {
  // Check if symbol is active
  {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    if (active_symbols_.find(symbol) == active_symbols_.end()) {
      return; // Symbol not subscribed
    }
  }
  
  // Limit depth
  auto limited_bids = bids;
  auto limited_asks = asks;
  if (max_depth_ > 0) {
    if (limited_bids.size() > static_cast<size_t>(max_depth_)) {
      limited_bids.resize(max_depth_);
    }
    if (limited_asks.size() > static_cast<size_t>(max_depth_)) {
      limited_asks.resize(max_depth_);
    }
  }
  
  // Update statistics
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.orderbooks_published++;
    stats_.last_update_time_us = timestamp_us;
  }
  
  // Publish immediately (no buffering for now)
  publish_orderbook(symbol, limited_bids, limited_asks, timestamp_us);
}

void QuoteServer::set_parser(std::unique_ptr<IExchangeParser> parser) {
  normalizer_->set_parser(std::move(parser));
}

void QuoteServer::publish_loop() {
  // This loop can be used for periodic publishing or heartbeat
  // For now, we publish immediately on updates
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void QuoteServer::publish_orderbook(const std::string& symbol,
                                  const std::vector<std::pair<double, double>>& bids,
                                  const std::vector<std::pair<double, double>>& asks,
                                  uint64_t timestamp_us) {
  // If protobuf enabled, publish proto snapshot; else use binary helper
#ifdef PROTO_ENABLED
  proto::OrderBookSnapshot snap;
  snap.set_exch(exchange_name_);
  snap.set_symbol(symbol);
  snap.set_timestamp_us(timestamp_us);
  for (const auto& [p, q] : bids) {
    auto* lvl = snap.add_bids();
    lvl->set_price(p);
    lvl->set_qty(q);
  }
  for (const auto& [p, q] : asks) {
    auto* lvl = snap.add_asks();
    lvl->set_price(p);
    lvl->set_qty(q);
  }
  std::string payload;
  snap.SerializeToString(&payload);
  std::string topic = "md." + exchange_name_ + "." + symbol;
  publisher_->publish(topic, payload);
#else
  try {
    size_t size = OrderBookBinaryHelper::calculate_size(symbol.length(), bids.size(), asks.size());
    std::vector<char> buffer(size);
    uint32_t sequence = normalizer_->get_next_sequence();
    OrderBookBinaryHelper::serialize(symbol, bids, asks, timestamp_us, sequence, buffer.data(), size);
    std::string topic = "md." + exchange_name_ + "." + symbol;
    publisher_->publish(topic, std::string(buffer.data(), buffer.size()));
  } catch (const std::exception& e) {
    std::cerr << "[QUOTE_SERVER] Publish error: " << e.what() << std::endl;
  }
#endif
  std::cout << "[QUOTE_SERVER] Published " << symbol 
            << " bids=" << bids.size() << " asks=" << asks.size() << std::endl;
}

QuoteServer::Stats QuoteServer::get_stats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void QuoteServer::reset_stats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = Stats{};
}
