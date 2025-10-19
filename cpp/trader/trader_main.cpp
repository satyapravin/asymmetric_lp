#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <string>
#include "../utils/config/process_config_manager.hpp"
#include "zmq_oms_adapter.hpp"
#include "zmq_mds_adapter.hpp"
#include "zmq_pms_adapter.hpp"
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"
#include "../proto/position.pb.h"

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

// Signal handler for clean shutdown
void signal_handler(int signal) {
    std::cout << "\n[TRADER] Received signal " << signal << ", shutting down..." << std::endl;
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
        std::cerr << "=== Trader Process ===" << std::endl;
        std::cerr << "Usage: ./trader -c <path/to/config.ini>" << std::endl;
        std::cerr << "Example: ./trader -c /etc/trader/trader_binance.ini" << std::endl;
        return 1;
    }

    // Load configuration
    config::ProcessConfigManager config;
    if (!config.load_config(config_file)) {
        std::cerr << "Failed to load configuration from: " << config_file << std::endl;
        return 1;
    }
    
    // Get ZMQ endpoints
    std::string order_pub_endpoint = config.get_string("zmq", "order_pub_endpoint", "tcp://127.0.0.1:6002");
    std::string order_sub_endpoint = config.get_string("zmq", "order_events_sub_endpoint", "tcp://127.0.0.1:6002");
    std::string market_data_sub_endpoint = config.get_string("zmq", "market_data_sub_endpoint", "tcp://127.0.0.1:6001");
    std::string position_sub_endpoint = config.get_string("zmq", "position_events_sub_endpoint", "tcp://127.0.0.1:6003");
    
    std::cout << "Starting Trader Process" << std::endl;
    std::cout << "Order endpoint: " << order_pub_endpoint << std::endl;
    std::cout << "Market data endpoint: " << market_data_sub_endpoint << std::endl;
    std::cout << "Position endpoint: " << position_sub_endpoint << std::endl;
    
    // Create ZMQ proxies
    auto oms_proxy = std::make_unique<ZMQOMS>(
        order_pub_endpoint, "trader_orders",
        order_sub_endpoint, "order_events"
    );
    
    auto md_proxy = std::make_unique<ZmqMDAdapter>(
        market_data_sub_endpoint, "market.binance.BTCUSDT", "binance"
    );
    
    auto pms_proxy = std::make_unique<ZmqPMSAdapter>(
        position_sub_endpoint, "position_updates"
    );
    
    // Set up market data callback
    md_proxy->on_snapshot = [](const OrderBookSnapshot& snapshot) {
        std::cout << "[MARKET_DATA] " << snapshot.exch << " " << snapshot.symbol 
                  << " bid=" << snapshot.bid_px << " ask=" << snapshot.ask_px << std::endl;
    };
    
    // Set up order event callback
    oms_proxy->set_event_callback([](const std::string& cl_ord_id,
                                   const std::string& exch,
                                   const std::string& symbol,
                                   uint32_t event_type,
                                   double fill_qty,
                                   double fill_price,
                                   const std::string& text) {
        std::cout << "[ORDER_EVENT] " << cl_ord_id << " " << exch << " " << symbol 
                  << " type=" << event_type << " qty=" << fill_qty << " price=" << fill_price 
                  << " " << text << std::endl;
    });
    
    // Set up position update callback
    pms_proxy->set_position_callback([](const proto::PositionUpdate& position) {
        std::cout << "[POSITION] " << position.symbol() << " qty=" << position.qty() 
                  << " avg_price=" << position.avg_price() << std::endl;
    });
    
    std::cout << "Trader running. Press Ctrl+C to stop." << std::endl;
    
    // Main loop
    while (g_running.load()) {
        // Poll for order events
        oms_proxy->poll_events();
        
        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Trader stopped." << std::endl;
    return 0;
}
