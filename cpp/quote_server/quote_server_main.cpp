#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <algorithm>
#include "../utils/config/config.hpp"
#include "quote_server.hpp"

static std::atomic<bool> running{true};

void signal_handler(int signal) {
  std::cout << "\n[QUOTE_SERVER] Received signal " << signal << ", shutting down..." << std::endl;
  running.store(false);
}

int main() {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  auto cfg = load_app_config();
  std::cout << "Starting Quote Server Framework" << std::endl;
  
  // Create quote server
  auto quote_server = std::make_unique<QuoteServer>(cfg.exchanges_csv, cfg.md_pub_endpoint);
  
  // Set up parser based on exchange
  std::string exchange = cfg.exchanges_csv;
  std::transform(exchange.begin(), exchange.end(), exchange.begin(), ::toupper);
  
  if (exchange == "BINANCE") {
    quote_server->set_parser(std::make_unique<BinanceParser>());
  } else if (exchange == "COINBASE") {
    quote_server->set_parser(std::make_unique<CoinbaseParser>());
  } else {
    // Default to mock parser for testing
    quote_server->set_parser(std::make_unique<MockParser>(cfg.symbol, 2000.0));
  }
  
  // Configure server
  quote_server->set_publish_rate_hz(20.0); // 20Hz publishing
  quote_server->set_max_depth(10); // 10 levels deep
  
  // Set websocket URL based on exchange
  std::string websocket_url;
  if (exchange == "BINANCE") {
    websocket_url = "wss://stream.binance.com:9443/ws/ethusdt@depth";
  } else if (exchange == "COINBASE") {
    websocket_url = "wss://ws-feed.exchange.coinbase.com";
  } else {
    // Mock URL for testing
    websocket_url = "ws://127.0.0.1:8080/mock";
  }
  
  quote_server->set_websocket_url(websocket_url);
  
  // Add symbols
  quote_server->add_symbol(cfg.symbol);
  
  // Start server and connect to exchange
  quote_server->start();
  quote_server->connect_to_exchange();
  
  std::cout << "Quote server running. Press Ctrl+C to stop." << std::endl;
  
  // Keep running until signal received
  while (running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Print stats every 5 seconds
    static auto last_stats_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
      auto stats = quote_server->get_stats();
      std::cout << "[STATS] Messages: " << stats.messages_processed 
                << " Orderbooks: " << stats.orderbooks_published
                << " Errors: " << stats.parse_errors << std::endl;
      last_stats_time = now;
    }
  }
  
  quote_server->stop();
  std::cout << "Quote server stopped." << std::endl;
  return 0;
}
