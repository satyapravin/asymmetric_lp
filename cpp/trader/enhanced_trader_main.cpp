#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <memory>
#include "market_making_strategy.hpp"
#include "models/glft_target.hpp"
#include "../utils/oms/exchange_oms_factory.hpp"
#include "../utils/oms/exchange_monitor.hpp"

// Global flag for clean shutdown
std::atomic<bool> g_running{true};
std::atomic<int> g_signal_count{0};

// Signal handler for clean shutdown
void signal_handler(int signal) {
  g_signal_count++;
  std::cout << "\n[ENHANCED_TRADER] Received signal " << signal << " (count: " << g_signal_count.load() << ")" << std::endl;
  
  if (g_signal_count.load() >= 2) {
    std::cout << "[ENHANCED_TRADER] Force shutdown after multiple signals" << std::endl;
    exit(1);
  }
  
  g_running.store(false);
}

int main(int argc, char** argv) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  std::cout << "=== Enhanced Market Making Trader ===" << std::endl;
  std::cout << "Features: Configuration-driven exchanges, monitoring, rich error handling" << std::endl;
  
  // Configuration
  std::string config_file = "config.enhanced.ini";
  if (argc > 1) {
    config_file = argv[1];
  }
  
  std::cout << "Loading configuration from: " << config_file << std::endl;
  
  // Load exchange configurations
  auto exchange_configs = ExchangeOMSFactory::load_exchanges_from_config(config_file);
  if (exchange_configs.empty()) {
    std::cerr << "No exchanges configured. Using default mock exchanges." << std::endl;
    
    // Fallback to default configuration
    ExchangeConfig binance_config;
    binance_config.name = "BINANCE";
    binance_config.type = "MOCK";
    binance_config.fill_probability = 0.8;
    binance_config.reject_probability = 0.1;
    binance_config.response_delay_ms = 150;
    exchange_configs.push_back(binance_config);
    
    ExchangeConfig deribit_config;
    deribit_config.name = "DERIBIT";
    deribit_config.type = "MOCK";
    deribit_config.fill_probability = 0.7;
    deribit_config.reject_probability = 0.15;
    deribit_config.response_delay_ms = 200;
    exchange_configs.push_back(deribit_config);
    
    ExchangeConfig grvt_config;
    grvt_config.name = "GRVT";
    grvt_config.type = "MOCK";
    grvt_config.fill_probability = 0.9;
    grvt_config.reject_probability = 0.05;
    grvt_config.response_delay_ms = 100;
    exchange_configs.push_back(grvt_config);
  }
  
  // Create monitoring system
  auto monitor = std::make_shared<ExchangeMonitor>();
  
  // Set up monitoring callbacks
  monitor->set_health_alert_callback([](const std::string& exchange, HealthStatus status) {
    std::cout << "[MONITOR] Health alert for " << exchange << ": " << static_cast<int>(status) << std::endl;
  });
  
  monitor->set_performance_alert_callback([](const std::string& exchange, const std::string& message) {
    std::cout << "[MONITOR] Performance alert for " << exchange << ": " << message << std::endl;
  });
  
  // Create GLFT model
  auto glft_model = std::make_shared<GlftTarget>();
  
  // Create unified market making strategy
  std::string symbol = "BTCUSDC-PERP";
  auto strategy = std::make_unique<MarketMakingStrategy>(
    symbol, glft_model, 
    "tcp://127.0.0.1:6001", "market_data",
    "tcp://127.0.0.1:6004", "positions",
    "tcp://127.0.0.1:6007", "inventory");
  
  // Create and register exchanges using factory
  std::cout << "\n=== Creating Exchanges ===" << std::endl;
  for (const auto& config : exchange_configs) {
    std::cout << "Creating exchange: " << config.name << " (type: " << config.type << ")" << std::endl;
    
    auto exchange_oms = ExchangeOMSFactory::create_exchange(config);
    if (exchange_oms) {
      strategy->register_exchange(config.name, exchange_oms);
      std::cout << "✓ Successfully registered " << config.name << std::endl;
    } else {
      std::cerr << "✗ Failed to create exchange " << config.name << std::endl;
    }
  }
  
  // Set up enhanced callbacks with monitoring
  strategy->set_order_event_callback([&monitor](const OrderEvent& event) {
    std::cout << "[CALLBACK] Order event: " << event.cl_ord_id << " " << to_string(event.type);
    if (event.type == OrderEventType::Fill) {
      std::cout << " fill_qty=" << event.fill_qty << " fill_price=" << event.fill_price;
      // Record fill in monitoring
      monitor->record_order_fill(event.exch, event.symbol, event.fill_qty);
    }
    std::cout << std::endl;
  });
  
  strategy->set_order_state_callback([](const OrderStateInfo& order_info) {
    std::cout << "[CALLBACK] Order state: " << order_info.cl_ord_id 
              << " -> " << to_string(order_info.state) << std::endl;
  });
  
  // Start strategy
  strategy->start();
  
  std::cout << "\nEnhanced trader running for " << symbol << std::endl;
  std::cout << "Exchanges: " << exchange_configs.size() << std::endl;
  std::cout << "Monitoring: Enabled" << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  
  // Simulate some market activity
  std::cout << "\n=== Simulating Market Activity ===" << std::endl;
  
  // Simulate orderbook updates
  std::vector<std::pair<double, double>> bids = {{50000.0, 0.1}};
  std::vector<std::pair<double, double>> asks = {{50001.0, 0.1}};
  strategy->on_orderbook_update(symbol, bids, asks, 
    std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  
  // Simulate inventory update
  std::cout << "\n=== Simulating Inventory Update (DeFi) ===" << std::endl;
  strategy->on_inventory_update(symbol, 0.1);
  
  // Wait for orders to process
  std::this_thread::sleep_for(std::chrono::seconds(3));
  
  // Show monitoring information
  std::cout << "\n=== Monitoring Summary ===" << std::endl;
  monitor->print_metrics_summary();
  monitor->print_health_summary();
  
  // Keep running until signal received
  auto last_metrics_time = std::chrono::steady_clock::now();
  const auto metrics_interval = std::chrono::seconds(10);
  
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Print metrics periodically
    auto now = std::chrono::steady_clock::now();
    if (now - last_metrics_time >= metrics_interval) {
      std::cout << "\n=== Periodic Monitoring Update ===" << std::endl;
      monitor->print_metrics_summary();
      last_metrics_time = now;
    }
  }
  
  // Cleanup
  std::cout << "\nShutting down enhanced trader..." << std::endl;
  
  try {
    strategy->stop();
  } catch (const std::exception& e) {
    std::cout << "[ENHANCED_TRADER] Error during cleanup: " << e.what() << std::endl;
  }
  
  // Final monitoring report
  std::cout << "\n=== Final Monitoring Report ===" << std::endl;
  monitor->print_metrics_summary();
  monitor->print_health_summary();
  
  std::cout << "Enhanced trader stopped." << std::endl;
  return 0;
}