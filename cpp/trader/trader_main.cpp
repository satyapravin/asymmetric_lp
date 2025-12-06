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
#include "../utils/logging/log_helper.hpp"

using namespace trader;

// Global variables for signal handling
std::atomic<bool> g_running{true};
std::unique_ptr<TraderLib> g_trader;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    LOG_INFO_COMP("TRADER", "Received signal " + std::to_string(signal) + ", shutting down...");
    g_running.store(false);
    
    if (g_trader) {
        g_trader->stop();
    }
}

int main(int argc, char** argv) {
    LOG_INFO_COMP("TRADER", "=== Trader Process ===");
    
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
    
    LOG_INFO_COMP("TRADER", "Config file: " + config_file);
    LOG_INFO_COMP("TRADER", "Strategy: " + strategy_name);
    LOG_INFO_COMP("TRADER", "Symbol: " + symbol);
    LOG_INFO_COMP("TRADER", "Exchange: " + exchange);
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    try {
        // Initialize configuration
        auto config_manager = std::make_unique<config::ProcessConfigManager>();
        if (!config_manager->load_config(config_file)) {
            LOG_ERROR_COMP("TRADER", "Failed to load configuration from " + config_file);
            return 1;
        }
        
        // Get configuration values
        std::string oms_publish_endpoint = config_manager->get_string("zmq.oms_publish_endpoint", "tcp://localhost:5557");
        std::string oms_subscribe_endpoint = config_manager->get_string("zmq.oms_subscribe_endpoint", "tcp://localhost:5558");
        std::string mds_subscribe_endpoint = config_manager->get_string("zmq.mds_subscribe_endpoint", "tcp://localhost:5555");
        std::string pms_subscribe_endpoint = config_manager->get_string("zmq.pms_subscribe_endpoint", "tcp://localhost:5556");
        
        LOG_INFO_COMP("TRADER", "OMS subscribe endpoint: " + oms_subscribe_endpoint);
        LOG_INFO_COMP("TRADER", "MDS subscribe endpoint: " + mds_subscribe_endpoint);
        LOG_INFO_COMP("TRADER", "PMS subscribe endpoint: " + pms_subscribe_endpoint);
        
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
            LOG_INFO_COMP("TRADER", "Market making strategy configured");
        } else {
            LOG_ERROR_COMP("TRADER", "Unknown strategy: " + strategy_name);
            return 1;
        }
        
        // Initialize the library
        if (!g_trader->initialize(config_file)) {
            LOG_ERROR_COMP("TRADER", "Failed to initialize trader library");
            return 1;
        }
        
        // Start the trader
        g_trader->start();
        
        LOG_INFO_COMP("TRADER", "Trader started successfully");
        LOG_INFO_COMP("TRADER", "Running " + strategy_name + " strategy on " + exchange + ":" + symbol);
        
        // Main processing loop
        while (g_running.load()) {
            // Check if trader is still running
            if (!g_trader->is_running()) {
                LOG_ERROR_COMP("TRADER", "Trader stopped unexpectedly");
                break;
            }
            
            // Print statistics every 30 seconds
            static auto last_stats_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 30) {
                const auto& stats = g_trader->get_statistics();
                std::string stats_msg = "Orders sent: " + std::to_string(stats.orders_sent.load()) +
                                       ", Orders cancelled: " + std::to_string(stats.orders_cancelled.load()) +
                                       ", Market data received: " + std::to_string(stats.market_data_received.load()) +
                                       ", Position updates: " + std::to_string(stats.position_updates.load()) +
                                       ", Balance updates: " + std::to_string(stats.balance_updates.load()) +
                                       ", Trade executions: " + std::to_string(stats.trade_executions.load()) +
                                       ", ZMQ messages received: " + std::to_string(stats.zmq_messages_received.load()) +
                                       ", ZMQ messages sent: " + std::to_string(stats.zmq_messages_sent.load());
                LOG_INFO_COMP("STATS", stats_msg);
                last_stats_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("TRADER", "Exception: " + std::string(e.what()));
        return 1;
    }
    
    // Cleanup
    if (g_trader) {
        g_trader->stop();
    }
    
    LOG_INFO_COMP("TRADER", "Trader stopped");
    return 0;
}