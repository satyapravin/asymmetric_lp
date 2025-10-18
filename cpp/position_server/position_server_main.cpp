#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <algorithm>
#include "../utils/config/config.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/pms/position_binary.hpp"
#include "../utils/pms/position_feed.hpp"
#include "position_server_factory.hpp"
#ifdef PROTO_ENABLED
#include "position.pb.h"
#endif

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

// Signal handler for clean shutdown
void signal_handler(int signal) {
  std::cout << "\n[POSITION_SERVER] Received signal " << signal << ", shutting down..." << std::endl;
  g_running.store(false);
}

static std::vector<char> build_mock_position_binary(const std::string& symbol,
                                                   const std::string& exch,
                                                   double qty,
                                                   double avg_price) {
  std::vector<char> buffer(PositionBinaryHelper::POSITION_SIZE);
  
  PositionBinaryHelper::serialize_position(symbol, exch, qty, avg_price, buffer.data());
  return buffer;
}

int main(int argc, char** argv) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // Load configuration from command line
  AppConfig cfg;
  std::string config_file;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
      config_file = argv[++i];
    } else if (arg.rfind("--config=", 0) == 0) {
      config_file = arg.substr(std::string("--config=").size());
    }
  }
  if (config_file.empty()) {
    std::cerr << "Usage: position_server -c <path/to/config.ini>" << std::endl;
    return 1;
  }

  // Read configuration
  load_from_ini(config_file, cfg);
  
  // Validate required configuration
  if (cfg.exchanges_csv.empty() || cfg.pos_pub_endpoint.empty()) {
    std::cerr << "Config missing required keys. Need EXCHANGES, POS_PUB_ENDPOINT." << std::endl;
    return 1;
  }
  
  std::cout << "Starting Position Server for exchange: " << cfg.exchanges_csv << std::endl;
  
  // Position publisher endpoint
  ZmqPublisher pub(cfg.pos_pub_endpoint);
  std::cout << "Position server publishing on " << cfg.pos_pub_endpoint << std::endl;

  // Create position feed based on configuration
  std::string exchange_type = cfg.exchanges_csv;
  std::string api_key = "";
  std::string api_secret = "";
  
  // Get API credentials from exchange-specific section
  for (const auto& sec : cfg.sections) {
    std::string sec_upper = sec.name;
    std::transform(sec_upper.begin(), sec_upper.end(), sec_upper.begin(), ::toupper);
    if (sec_upper == exchange_type) {
      for (const auto& [k, v] : sec.entries) {
        if (k == "API_KEY") api_key = v;
        else if (k == "API_SECRET") api_secret = v;
      }
    }
  }
  
  // Fallback to environment variables if not in config
  if (api_key.empty() && const char* env_key = std::getenv("POSITION_API_KEY")) {
    api_key = env_key;
  }
  if (api_secret.empty() && const char* env_secret = std::getenv("POSITION_API_SECRET")) {
    api_secret = env_secret;
  }
  
  auto position_feed = PositionServerFactory::create_from_string(exchange_type, api_key, api_secret);
  
  // Set up position update callback
  position_feed->on_position_update = [&pub, &cfg](const std::string& symbol,
                                                   const std::string& exch,
                                                   double qty,
                                                   double avg_price) {
#ifdef PROTO_ENABLED
    proto::PositionUpdate upd;
    upd.set_exch(exch);
    upd.set_symbol(symbol);
    upd.set_qty(qty);
    upd.set_avg_price(avg_price);
    upd.set_timestamp_us(0);
    std::string out;
    upd.SerializeToString(&out);
    std::string topic = "pos." + exch + "." + symbol;
    pub.publish(topic, out);
#else
    auto binary_data = build_mock_position_binary(symbol, exch, qty, avg_price);
    std::string topic = "pos." + exch + "." + symbol;
    pub.publish(topic, std::string(binary_data.data(), binary_data.size()));
#endif
    
    std::cout << "[POSITION] " << exch << " " << symbol 
              << " qty=" << qty << " avg_price=" << avg_price << std::endl;
  };
  
  // Connect to position feed
  std::string account = "MM_ACCOUNT"; // Could be from config
  if (!position_feed->connect(account)) {
    std::cerr << "Failed to connect to position feed" << std::endl;
    return 1;
  }
  
  std::cout << "Position server running. Press Ctrl+C to stop." << std::endl;
  
  // Keep running until signal received
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  position_feed->disconnect();
  std::cout << "Position server stopped." << std::endl;
  return 0;
}