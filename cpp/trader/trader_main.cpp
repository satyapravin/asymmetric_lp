#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <memory>
#include "trader_lib.hpp"
#include "../strategies/mm_strategy/market_making_strategy.hpp"
#include "../strategies/mm_strategy/models/glft_target.hpp"
#include "../trader/zmq_oms_adapter.hpp"
#include "../trader/zmq_mds_adapter.hpp"
#include "../trader/zmq_pms_adapter.hpp"
#include "../utils/config/process_config_manager.hpp"

using namespace trader;

// Global variables for signal handling
std::atomic<bool> g_running{true};
std::unique_ptr<TraderLib> g_trader;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "[TRADER] Received signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
    
    if (g_trader) {
        g_trader->stop();
    }
}

int main(int argc, char** argv) {
    std::cout << "=== Trader Process ===" << std::endl;
    
    // Parse command line arguments
    std::string config_file = "trader.ini";
    std::string strategy_name = "market_making";
    std::string symbol = "BTCUSDT";
    std::string exchange = "BINANCE";
    
    if (argc > 1) {
        config_file = argv[1];
    }
    if (argc > 2) {
        strategy_name = argv[2];
    }
    if (argc > 3) {
        symbol = argv[3];
    }
    if (argc > 4) {
        exchange = argv[4];
    }
    
    std::cout << "Config file: " << config_file << std::endl;
    std::cout << "Strategy: " << strategy_name << std::endl;
    std::cout << "Symbol: " << symbol << std::endl;
    std::cout << "Exchange: " << exchange << std::endl;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    try {
        // Initialize configuration
        auto config_manager = std::make_unique<config::ProcessConfigManager>();
        if (!config_manager->load_config(config_file)) {
            std::cerr << "Failed to load configuration from " << config_file << std::endl;
            return 1;
        }
        
        // Get configuration values
        std::string oms_publish_endpoint = config_manager->get_string("zmq.oms_publish_endpoint", "tcp://localhost:5557");
        std::string oms_subscribe_endpoint = config_manager->get_string("zmq.oms_subscribe_endpoint", "tcp://localhost:5558");
        std::string mds_subscribe_endpoint = config_manager->get_string("zmq.mds_subscribe_endpoint", "tcp://localhost:5555");
        std::string pms_subscribe_endpoint = config_manager->get_string("zmq.pms_subscribe_endpoint", "tcp://localhost:5556");
        
        std::cout << "OMS subscribe endpoint: " << oms_subscribe_endpoint << std::endl;
        std::cout << "MDS subscribe endpoint: " << mds_subscribe_endpoint << std::endl;
        std::cout << "PMS subscribe endpoint: " << pms_subscribe_endpoint << std::endl;
        
        // Initialize ZMQ adapters
        auto oms_adapter = std::make_shared<ZmqOMSAdapter>(oms_publish_endpoint, "orders", oms_subscribe_endpoint, "order_events");
        
        auto mds_adapter = std::make_shared<ZmqMDSAdapter>(mds_subscribe_endpoint, "market_data", "binance");
        
        auto pms_adapter = std::make_shared<ZmqPMSAdapter>(pms_subscribe_endpoint, "position_updates");
        
        // Initialize trader library
        g_trader = std::make_unique<TraderLib>();
        
        // Configure the library
        g_trader->set_symbol(symbol);
        g_trader->set_exchange(exchange);
        g_trader->set_oms_adapter(oms_adapter);
        g_trader->set_mds_adapter(mds_adapter);
        g_trader->set_pms_adapter(pms_adapter);
        
        // Create and set strategy
        if (strategy_name == "market_making") {
            // Create GLFT model for market making strategy
            auto glft_model = std::make_shared<GlftTarget>();
            auto strategy = std::make_shared<MarketMakingStrategy>(symbol, glft_model);
            
            g_trader->set_strategy(strategy);
            std::cout << "[TRADER] Market making strategy configured" << std::endl;
        } else {
            std::cerr << "Unknown strategy: " << strategy_name << std::endl;
            return 1;
        }
        
        // Initialize the library
        if (!g_trader->initialize(config_file)) {
            std::cerr << "Failed to initialize trader library" << std::endl;
            return 1;
        }
        
        // Start the trader
        g_trader->start();
        
        std::cout << "[TRADER] Trader started successfully" << std::endl;
        std::cout << "[TRADER] Running " << strategy_name << " strategy on " << exchange << ":" << symbol << std::endl;
        
        // Main processing loop
        while (g_running.load()) {
            // Check if trader is still running
            if (!g_trader->is_running()) {
                std::cerr << "[TRADER] Trader stopped unexpectedly" << std::endl;
                break;
            }
            
            // Print statistics every 30 seconds
            static auto last_stats_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 30) {
                const auto& stats = g_trader->get_statistics();
                std::cout << "[STATS] Orders sent: " << stats.orders_sent.load()
                          << ", Orders cancelled: " << stats.orders_cancelled.load()
                          << ", Market data received: " << stats.market_data_received.load()
                          << ", Position updates: " << stats.position_updates.load()
                          << ", Balance updates: " << stats.balance_updates.load()
                          << ", Trade executions: " << stats.trade_executions.load()
                          << ", ZMQ messages received: " << stats.zmq_messages_received.load()
                          << ", ZMQ messages sent: " << stats.zmq_messages_sent.load() << std::endl;
                last_stats_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADER] Exception: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    if (g_trader) {
        g_trader->stop();
    }
    
    std::cout << "[TRADER] Trader stopped" << std::endl;
    return 0;
}