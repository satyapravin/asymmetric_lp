#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
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

int main() {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  auto cfg = load_app_config();
  
  // Position publisher endpoint
  std::string pos_pub_endpoint = "tcp://127.0.0.1:6004"; // Different port for positions
  ZmqPublisher pub(pos_pub_endpoint);
  std::cout << "Position server publishing on " << pos_pub_endpoint << std::endl;

  // Create position feed based on configuration
  std::string exchange_type = "MOCK";  // Default to mock
  std::string api_key = "";
  std::string api_secret = "";
  
  // TODO: Read from config file
  // For now, use environment variables or command line args
  if (const char* env_exchange = std::getenv("POSITION_EXCHANGE")) {
    exchange_type = env_exchange;
  }
  if (const char* env_key = std::getenv("POSITION_API_KEY")) {
    api_key = env_key;
  }
  if (const char* env_secret = std::getenv("POSITION_API_SECRET")) {
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
