#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <memory>
#include "market_making_strategy.hpp"
#include "models/glft_target.hpp"
#include "../utils/oms/mock_exchange_oms.hpp"

// Global flag for clean shutdown
std::atomic<bool> g_running{true};
std::atomic<int> g_signal_count{0};

// Signal handler for clean shutdown
void signal_handler(int signal) {
  g_signal_count.fetch_add(1);
  std::cout << "\n[ENHANCED_TRADER] Received signal " << signal << " (count: " << g_signal_count.load() << "), shutting down..." << std::endl;
  g_running.store(false);
  
  // If we get multiple signals, force exit
  if (g_signal_count.load() >= 2) {
    std::cout << "[ENHANCED_TRADER] Force exit after multiple signals" << std::endl;
    std::exit(1);
  }
}

int main() {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  std::cout << "=== Unified Market Making Trader ===" << std::endl;
  
  // Configuration
  std::string symbol = "BTCUSDC-PERP";
  std::string md_endpoint = "tcp://127.0.0.1:6001";
  std::string md_topic = "market_data";
  std::string pos_endpoint = "tcp://127.0.0.1:6004";
  std::string pos_topic = "positions";
  std::string inventory_endpoint = "tcp://127.0.0.1:6007"; // DeFi inventory feed
  std::string inventory_topic = "inventory";
  
  // Create GLFT model (mock for now)
  auto glft_model = std::make_shared<GlftTarget>();
  
  // Create unified market making strategy
  auto strategy = std::make_unique<MarketMakingStrategy>(
    symbol, glft_model, md_endpoint, md_topic, pos_endpoint, pos_topic, 
    inventory_endpoint, inventory_topic);
  
  // Register mock exchanges
  auto binance_oms = std::make_shared<MockExchangeOMS>("BINANCE", 0.8, 0.1, std::chrono::milliseconds(150));
  auto deribit_oms = std::make_shared<MockExchangeOMS>("DERIBIT", 0.7, 0.15, std::chrono::milliseconds(200));
  auto grvt_oms = std::make_shared<MockExchangeOMS>("GRVT", 0.9, 0.05, std::chrono::milliseconds(100));
  
  strategy->register_exchange("BINANCE", binance_oms);
  strategy->register_exchange("DERIBIT", deribit_oms);
  strategy->register_exchange("GRVT", grvt_oms);
  
  // Set up callbacks
  strategy->set_order_event_callback([](const OrderEvent& event) {
    std::cout << "[CALLBACK] Order event: " << event.cl_ord_id 
              << " " << to_string(event.type);
    if (event.type == OrderEventType::Fill) {
      std::cout << " fill_qty=" << event.fill_qty << " fill_price=" << event.fill_price;
    }
    std::cout << std::endl;
  });
  
  strategy->set_order_state_callback([](const OrderStateInfo& order_info) {
    std::cout << "[CALLBACK] Order state: " << order_info.cl_ord_id 
              << " -> " << to_string(order_info.state) << std::endl;
  });
  
  // Configure strategy
  strategy->set_min_spread_bps(5.0);
  strategy->set_max_position_size(10.0);
  strategy->set_quote_size(0.1);
  
  // Start strategy
  strategy->start();
  
  // Connect to exchanges
  bool connected = true; // Assume connected for now
  if (!connected) {
    std::cout << "Warning: Some exchanges failed to connect" << std::endl;
  }
  
  std::cout << "Unified trader running for " << symbol << std::endl;
  std::cout << "Market data endpoint: " << md_endpoint << std::endl;
  std::cout << "Inventory endpoint: " << inventory_endpoint << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  
  // Simulate some market data to trigger quotes
  std::cout << "\n=== Simulating Market Data ===" << std::endl;
  
  std::vector<std::pair<double, double>> bids = {{50000.0, 0.5}, {49999.0, 1.0}, {49998.0, 2.0}};
  std::vector<std::pair<double, double>> asks = {{50001.0, 0.5}, {50002.0, 1.0}, {50003.0, 2.0}};
  
  strategy->on_orderbook_update(symbol, bids, asks, 
    std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  
  // Simulate position update
  std::cout << "\n=== Simulating Inventory Update (DeFi) ===" << std::endl;
  strategy->on_inventory_update(symbol, 0.1); // Small inventory delta
  
  // Wait for orders to process
  std::this_thread::sleep_for(std::chrono::seconds(3));
  
  // Show statistics
  std::cout << "\n=== Order Statistics ===" << std::endl;
  auto stats = strategy->get_order_statistics();
  std::cout << "Total orders: " << stats.total_orders << std::endl;
  std::cout << "Filled orders: " << stats.filled_orders << std::endl;
  std::cout << "Cancelled orders: " << stats.cancelled_orders << std::endl;
  std::cout << "Rejected orders: " << stats.rejected_orders << std::endl;
  std::cout << "Total volume: " << stats.total_volume << std::endl;
  std::cout << "Filled volume: " << stats.filled_volume << std::endl;
  
  // Show active orders
  std::cout << "\n=== Active Orders ===" << std::endl;
  auto active_orders = strategy->get_active_orders();
  if (active_orders.empty()) {
    std::cout << "No active orders" << std::endl;
  } else {
    for (const auto& order_info : active_orders) {
      std::cout << order_info.cl_ord_id << " " << order_info.symbol 
                << " " << to_string(order_info.side) << " " << order_info.qty 
                << " @ " << order_info.price << " [" << to_string(order_info.state) << "]" << std::endl;
    }
  }
  
  // Test order management
  std::cout << "\n=== Testing Order Management ===" << std::endl;
  
  // Submit a test order
  Order test_order;
  test_order.cl_ord_id = "TEST_MANUAL_001";
  test_order.exch = "BINANCE";
  test_order.symbol = symbol;
  test_order.side = Side::Buy;
  test_order.qty = 0.05;
  test_order.price = 49950.0;
  test_order.is_market = false;
  
  strategy->submit_order(test_order);
  
  // Wait a bit
  std::this_thread::sleep_for(std::chrono::seconds(2));
  
  // Check order state
  auto order_state = strategy->get_order_state("TEST_MANUAL_001");
  if (!order_state.cl_ord_id.empty()) {
    std::cout << "Test order state: " << to_string(order_state.state) << std::endl;
    
    // Try to cancel it (with timeout)
    std::cout << "Attempting to cancel test order..." << std::endl;
    strategy->cancel_order("TEST_MANUAL_001");
    
    // Wait briefly for cancellation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Cancellation attempt completed." << std::endl;
  }
  
  // Keep running until signal received
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Check more frequently
  }
  
  // Cleanup
  std::cout << "\nShutting down unified trader..." << std::endl;
  
  // Set a timeout for cleanup
  auto cleanup_start = std::chrono::steady_clock::now();
  auto cleanup_timeout = std::chrono::seconds(5);
  
  try {
    strategy->stop();
  } catch (const std::exception& e) {
    std::cout << "[UNIFIED_TRADER] Error during cleanup: " << e.what() << std::endl;
  }
  
  auto cleanup_duration = std::chrono::steady_clock::now() - cleanup_start;
  if (cleanup_duration > cleanup_timeout) {
    std::cout << "[UNIFIED_TRADER] Cleanup took longer than expected" << std::endl;
  }
  
  std::cout << "Unified trader stopped." << std::endl;
  return 0;
}
