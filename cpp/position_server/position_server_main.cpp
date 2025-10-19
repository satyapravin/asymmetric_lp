#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <algorithm>
#include "../utils/config/process_config_manager.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../exchanges/i_exchange_pms.hpp"
#include "../exchanges/pms_factory.hpp"
#include "../proto/position.pb.h"

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

// Signal handler for clean shutdown
void signal_handler(int signal) {
  std::cout << "\n[POSITION_SERVER] Received signal " << signal << ", shutting down..." << std::endl;
  g_running.store(false);
}


int main(int argc, char** argv) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // Parse command line arguments
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
    std::cerr << "=== Position Server Process ===" << std::endl;
    std::cerr << "Usage: ./position_server -c <path/to/config.ini>" << std::endl;
    std::cerr << "Example: ./position_server -c /etc/position_server/position_server_binance.ini" << std::endl;
    return 1;
  }

  // Load configuration
  config::ProcessConfigManager config;
  if (!config.load_config(config_file)) {
    std::cerr << "Failed to load configuration from: " << config_file << std::endl;
    return 1;
  }
  
  // Get exchange name and credentials
  std::string exchange_name = config.get_string("process", "exchange_name", "binance");
  std::string api_key = config.get_string("exchange", "api_key", "");
  std::string api_secret = config.get_string("exchange", "api_secret", "");
  
  // Get ZMQ publisher endpoint
  std::string position_pub_endpoint = config.get_string("zmq", "position_events_pub_endpoint", "tcp://127.0.0.1:6003");
  
  std::cout << "Starting Position Server for exchange: " << exchange_name << std::endl;
  std::cout << "Position server publishing on " << position_pub_endpoint << std::endl;
  
  // Create ZMQ publisher
  ZmqPublisher publisher(position_pub_endpoint);
  
  // Create exchange-specific PMS using factory
  auto pms = PMSFactory::create_pms(exchange_name);
  if (!pms) {
    std::cerr << "Failed to create PMS for exchange: " << exchange_name << std::endl;
    return 1;
  }
  
  // Set authentication credentials
  pms->set_auth_credentials(api_key, api_secret);
  
  // Set up position update callback
  pms->set_position_update_callback([&publisher, &exchange_name](const proto::PositionUpdate& position) {
    // Serialize position update
    std::string serialized_position;
    if (!position.SerializeToString(&serialized_position)) {
      std::cerr << "[POSITION_SERVER] Failed to serialize position update" << std::endl;
      return;
    }
    
    // Create topic: pos.<exchange>.<symbol>
    std::string topic = "pos." + exchange_name + "." + position.symbol();
    
    // Publish to ZMQ
    publisher.publish(topic, serialized_position);
    
    std::cout << "[POSITION] " << exchange_name << " " << position.symbol() 
              << " qty=" << position.qty() << " avg_price=" << position.avg_price() << std::endl;
  });
  
  // Connect to exchange
  if (!pms->connect()) {
    std::cerr << "Failed to connect to exchange: " << exchange_name << std::endl;
    return 1;
  }
  
  std::cout << "Position server running. Press Ctrl+C to stop." << std::endl;
  
  // Keep running until signal received
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check connection health
    if (!pms->is_connected()) {
      std::cerr << "[POSITION_SERVER] Connection lost, attempting to reconnect..." << std::endl;
      pms->disconnect();
      std::this_thread::sleep_for(std::chrono::seconds(5));
      if (!pms->connect()) {
        std::cerr << "[POSITION_SERVER] Reconnection failed" << std::endl;
      }
    }
  }
  
  // Clean shutdown
  pms->disconnect();
  std::cout << "Position server stopped." << std::endl;
  return 0;
}