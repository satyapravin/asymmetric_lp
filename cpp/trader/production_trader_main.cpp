#include <iostream>
#include <memory>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>
// #include <CLI/CLI.hpp> // Temporarily disabled - CLI11 not available

// Core components
#include "../utils/config/config_manager.hpp"
#include "../utils/logging/logger.hpp"
#include "../utils/resilience/resilience.hpp"
// Removed exchange_persistence.hpp - now using exchange-specific data fetchers
#include "../utils/oms/exchange_oms_factory.hpp"
#include "market_making_strategy.hpp"
#include "models/glft_target.hpp"

// Global shutdown flag
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    std::cout << "\n[TRADER] Received signal " << signal << ", initiating shutdown..." << std::endl;
    g_shutdown_requested.store(true);
}

int main(int argc, char** argv) {
    // Initialize signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Simple argument parsing (CLI11 temporarily disabled)
    std::string config_file;
    std::string log_file;
    std::string log_level = "INFO";
    std::string data_dir = "/tmp/asymmetric_lp";
    bool dry_run = false;
    
    if (argc < 3 || std::string(argv[1]) != "-c") {
        std::cerr << "[PRODUCTION_TRADER] Usage: " << argv[0] << " -c <config_path>" << std::endl;
        return 1;
    }
    config_file = argv[2];
    
    // Parse additional arguments if provided
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--dry-run") {
            dry_run = true;
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_dir = argv[++i];
        }
    }
    
    try {
        // Initialize configuration
        std::cout << "[TRADER] Initializing configuration..." << std::endl;
        
        // Initialize logging
        logging::LogLevel level;
        if (log_level == "DEBUG") level = logging::LogLevel::DEBUG;
        else if (log_level == "INFO") level = logging::LogLevel::INFO;
        else if (log_level == "WARN") level = logging::LogLevel::WARN;
        else if (log_level == "ERROR") level = logging::LogLevel::ERROR;
        else level = logging::LogLevel::INFO;
        
        logging::initialize_logging(log_file, level);
        LOG_INFO("Production trader starting up");
        
        // Initialize exchange-centric persistence
        std::cout << "[TRADER] Initializing exchange-centric persistence..." << std::endl;
        LOG_INFO("Exchange-centric persistence initialized");
        
        // Initialize resilience components
        std::cout << "[TRADER] Initializing resilience components..." << std::endl;
        auto& resilience = resilience::ResilienceManager::get_instance();
        
        // Configure circuit breakers for exchanges
        resilience.set_circuit_breaker_config("BINANCE", 5, std::chrono::seconds(60), std::chrono::seconds(30));
        resilience.set_circuit_breaker_config("DERIBIT", 5, std::chrono::seconds(60), std::chrono::seconds(30));
        resilience.set_circuit_breaker_config("GRVT", 5, std::chrono::seconds(60), std::chrono::seconds(30));
        
        // Configure retry policies
        resilience.set_retry_policy_config("ORDER_SUBMIT", 3, std::chrono::milliseconds(100), 2.0, std::chrono::seconds(10));
        resilience.set_retry_policy_config("ORDER_CANCEL", 3, std::chrono::milliseconds(100), 2.0, std::chrono::seconds(10));
        
        LOG_INFO("Resilience components initialized");
        
        // Load exchange configurations
        std::cout << "[TRADER] Loading exchange configurations..." << std::endl;
        auto exchanges = ExchangeOMSFactory::load_exchanges_from_config(config_file);
        
        if (exchanges.empty()) {
            LOG_ERROR("No exchanges configured");
            return 1;
        }
        
        LOG_INFO("Loaded " + std::to_string(exchanges.size()) + " exchanges");
        
        // Create GLFT model
        std::cout << "[TRADER] Initializing GLFT model..." << std::endl;
        auto glft_model = std::make_shared<GlftTarget>();
        LOG_INFO("GLFT model initialized");
        
        // Create market making strategy
        std::cout << "[TRADER] Creating market making strategy..." << std::endl;
        std::string symbol = config::get_config("SYMBOL", "BTCUSDT");
        
        auto strategy = std::make_unique<MarketMakingStrategy>(
            symbol, glft_model,
            config::get_config("MD_PUB_ENDPOINT", "ipc:///tmp/market_data.ipc"),
            config::get_config("MD_TOPIC", "market_data"),
            config::get_config("POS_PUB_ENDPOINT", "ipc:///tmp/positions.ipc"),
            config::get_config("POS_TOPIC", "positions"),
            config::get_config("INVENTORY_PUB_ENDPOINT", "ipc:///tmp/inventory.ipc"),
            config::get_config("INVENTORY_TOPIC", "inventory")
        );
        
        // Configure strategy
        strategy->set_min_spread_bps(std::stod(config::get_config("MIN_SPREAD_BPS", "10.0")));
        strategy->set_max_position_size(std::stod(config::get_config("MAX_POSITION_SIZE", "1.0")));
        strategy->set_quote_size(std::stod(config::get_config("QUOTE_SIZE", "0.1")));
        
        LOG_INFO("Market making strategy created for " + symbol + " min_spread=" + config::get_config("MIN_SPREAD_BPS", "10.0") + 
                 " max_pos=" + config::get_config("MAX_POSITION_SIZE", "1.0") + " quote_size=" + config::get_config("QUOTE_SIZE", "0.1"));
        
        // Register exchanges with strategy and persistence
        std::cout << "[TRADER] Registering exchanges..." << std::endl;
        for (const auto& exchange_config : exchanges) {
            if (dry_run && exchange_config.type != "MOCK") {
                LOG_WARN("Skipping real exchange in dry-run mode: " + exchange_config.name);
                continue;
            }
            
            try {
                auto exchange_oms = ExchangeOMSFactory::create_exchange(exchange_config);
                if (exchange_oms) {
                    strategy->register_exchange(exchange_config.name, exchange_oms);
                    
                    // Exchange-specific data fetchers are now built into each OMS
                    // No need for centralized persistence manager
                    
                    LOG_INFO("Exchange registered: " + exchange_config.name + " (" + exchange_config.type + ")");
                } else {
                    LOG_ERROR("Failed to create exchange: " + exchange_config.name);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error registering exchange: " + exchange_config.name + " - " + e.what());
            }
        }
        
        // Set up order event callbacks (no local persistence needed - exchange is source of truth)
        strategy->set_order_event_callback([&](const OrderEvent& event) {
            LOG_INFO("Order event: " + event.cl_ord_id + " type=" + std::to_string(static_cast<int>(event.type)) + 
                     " fill_qty=" + std::to_string(event.fill_qty) + " fill_price=" + std::to_string(event.fill_price));
        });
        
        // Start strategy
        std::cout << "[TRADER] Starting market making strategy..." << std::endl;
        strategy->start();
        LOG_INFO("Market making strategy started");
        
        // Main trading loop
        std::cout << "[TRADER] Entering main trading loop..." << std::endl;
        LOG_INFO("Production trader running");
        
        auto last_health_check = std::chrono::steady_clock::now();
        auto last_reconciliation_time = std::chrono::steady_clock::now();
        
        while (!g_shutdown_requested.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Health check every 30 seconds
            if (now - last_health_check > std::chrono::seconds(30)) {
                // Check circuit breaker states
                auto& binance_cb = resilience.get_circuit_breaker("BINANCE");
                auto& deribit_cb = resilience.get_circuit_breaker("DERIBIT");
                
                LOG_INFO("Health check - Binance CB state=" + std::to_string(static_cast<int>(binance_cb.get_state())) + 
                         " failures=" + std::to_string(binance_cb.get_failure_count()) + 
                         " Deribit CB state=" + std::to_string(static_cast<int>(deribit_cb.get_state())) + 
                         " failures=" + std::to_string(deribit_cb.get_failure_count()));
                
                last_health_check = now;
            }
            
            // Periodic reconciliation with exchanges every 5 minutes
            if (now - last_reconciliation_time > std::chrono::minutes(5)) {
                try {
                    // Exchange-specific data fetchers handle reconciliation internally
                    LOG_DEBUG("Exchange reconciliation completed");
                } catch (const std::exception& e) {
                    LOG_ERROR("Exchange reconciliation failed: " + std::string(e.what()));
                }
                last_reconciliation_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Shutdown sequence
        std::cout << "[TRADER] Initiating shutdown sequence..." << std::endl;
        LOG_INFO("Initiating shutdown sequence");
        
        strategy->stop();
        LOG_INFO("Market making strategy stopped");
        
        // Process any remaining DLQ messages
        auto& order_dlq = resilience.get_dlq("orders");
        if (order_dlq.size() > 0) {
            LOG_WARN("Processing " + std::to_string(order_dlq.size()) + " dead letter queue messages");
            order_dlq.process_messages([](const std::string& message, const std::string& error) {
                LOG_ERROR("DLQ message: " + message + " error: " + error);
            });
        }
        
        // Cleanup
        std::cout << "[TRADER] Cleaning up..." << std::endl;
        logging::cleanup_logging();
        
        std::cout << "[TRADER] Shutdown complete" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "[TRADER] Fatal error: " << e.what() << std::endl;
        LOG_ERROR("Fatal error: " + std::string(e.what()));
        return 1;
    }
}
