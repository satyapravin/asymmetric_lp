#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <algorithm>
#include <cstdlib>
#include "../utils/config/config.hpp"
#include "quote_server.hpp"

static std::atomic<bool> running{true};

void signal_handler(int signal) {
  std::cout << "\n[QUOTE_SERVER] Received signal " << signal << ", shutting down..." << std::endl;
  running.store(false);
}

int main(int argc, char** argv) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // Load quote-server specific config from CLI only (no env, no defaults)
  AppConfig cfg;
  std::string qs_ini;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
      qs_ini = argv[++i];
    } else if (arg.rfind("--config=", 0) == 0) {
      qs_ini = arg.substr(std::string("--config=").size());
    }
  }
  if (qs_ini.empty()) {
    std::cerr << "Usage: quote_server -c <path/to/config.ini>" << std::endl;
    return 1;
  }

  // Read per-process config (required)
  load_from_ini(qs_ini, cfg);

  // Validate required keys
  if (cfg.exchanges_csv.empty() || cfg.symbol.empty() || cfg.md_pub_endpoint.empty()) {
    std::cerr << "Config missing required keys. Need EXCHANGES, SYMBOL, MD_PUB_ENDPOINT." << std::endl;
    return 1;
  }
  std::cout << "Starting Quote Server Framework" << std::endl;
  
  // Create quote server
  auto quote_server = std::make_unique<QuoteServer>(cfg.exchanges_csv, cfg.md_pub_endpoint);
  
  // Set up parser via factory based on config (PARSER or EXCHANGES)
  std::string exchange = cfg.exchanges_csv;
  std::transform(exchange.begin(), exchange.end(), exchange.begin(), ::toupper);
  std::string parser_name = cfg.parser.empty() ? exchange : cfg.parser;
  quote_server->set_parser(create_exchange_parser(parser_name, cfg.symbol));
  
  // Configure server from config (fallback to sensible defaults if unset)
  if (cfg.publish_rate_hz > 0) quote_server->set_publish_rate_hz(cfg.publish_rate_hz); else quote_server->set_publish_rate_hz(20.0);
  if (cfg.max_depth > 0) quote_server->set_max_depth(cfg.max_depth); else quote_server->set_max_depth(10);
  if (cfg.snapshot_only && cfg.max_depth <= 0 && cfg.book_depth > 0) {
    quote_server->set_max_depth(cfg.book_depth);
  }
  
  // For plugin-based exchanges, websocket URL may be implicit; do not hard-require
  if (!cfg.websocket_url.empty()) {
    quote_server->set_websocket_url(cfg.websocket_url);
  } else {
    // Use a sensible default for Binance perps combined stream
    if (exchange == "BINANCE") {
      quote_server->set_websocket_url("wss://fstream.binance.com/stream");
    }
  }
  
  // Configure exchange-specific section (channels, symbols, plugin path, etc.)
  for (const auto& sec : cfg.sections) {
    std::string sec_upper = sec.name;
    std::transform(sec_upper.begin(), sec_upper.end(), sec_upper.begin(), ::toupper);
    if (sec_upper == exchange && quote_server) {
      quote_server->set_exchange_config(sec.entries);
      // If plugin path is specified for this exchange, export an env var for factory
      for (const auto& kv : sec.entries) {
        if (kv.first == "PLUGIN_PATH") {
          if (exchange == "BINANCE") setenv("BINANCE_PLUGIN_PATH", kv.second.c_str(), 1);
        }
      }
    }
  }

  // Add symbols: prefer exchange-specific sections if present, else fallback to SYMBOL
  bool added_any = false;
  for (const auto& sec : cfg.sections) {
    std::string sec_upper = sec.name;
    std::transform(sec_upper.begin(), sec_upper.end(), sec_upper.begin(), ::toupper);
    if (sec_upper == exchange) {
      for (const auto& [k, v] : sec.entries) {
        if (k == "SYMBOL") { quote_server->add_symbol(v); added_any = true; }
      }
    }
  }
  if (!added_any && !cfg.symbol.empty()) {
    quote_server->add_symbol(cfg.symbol);
  }
  
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
