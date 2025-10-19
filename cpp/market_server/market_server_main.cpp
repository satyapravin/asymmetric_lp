#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <algorithm>
#include "../utils/config/process_config_manager.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../exchanges/i_exchange_subscriber.hpp"
#include "../exchanges/subscriber_factory.hpp"
#include "../proto/market_data.pb.h"

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

// Signal handler for clean shutdown
void signal_handler(int signal) {
    std::cout << "\n[MARKET_SERVER] Received signal " << signal << ", shutting down..." << std::endl;
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
        std::cerr << "=== Market Server Process ===" << std::endl;
        std::cerr << "Usage: ./market_server -c <path/to/config.ini>" << std::endl;
        std::cerr << "Example: ./market_server -c /etc/market_server/market_server_binance.ini" << std::endl;
        return 1;
    }

    // Load configuration
    config::ProcessConfigManager config;
    if (!config.load_config(config_file)) {
        std::cerr << "Failed to load configuration from: " << config_file << std::endl;
        return 1;
    }
    
    // Get exchange name and symbols
    std::string exchange_name = config.get_string("process", "exchange_name", "binance");
    std::string symbols_str = config.get_string("market_data", "symbols", "BTCUSDT");
    
    // Get ZMQ publisher endpoint
    std::string market_data_pub_endpoint = config.get_string("zmq", "market_data_pub_endpoint", "tcp://127.0.0.1:6001");
    
    std::cout << "Starting Market Server for exchange: " << exchange_name << std::endl;
    std::cout << "Market server publishing on " << market_data_pub_endpoint << std::endl;
    
    // Create ZMQ publisher
    ZmqPublisher publisher(market_data_pub_endpoint);
    
    // Create exchange-specific subscriber using factory
    auto subscriber = SubscriberFactory::create_subscriber(exchange_name);
    if (!subscriber) {
        std::cerr << "Failed to create subscriber for exchange: " << exchange_name << std::endl;
        return 1;
    }
    
    // Set up orderbook callback
    subscriber->set_orderbook_callback([&publisher, &exchange_name](const proto::OrderBookSnapshot& orderbook) {
        // Serialize orderbook snapshot
        std::string serialized_orderbook;
        if (!orderbook.SerializeToString(&serialized_orderbook)) {
            std::cerr << "[MARKET_SERVER] Failed to serialize orderbook snapshot" << std::endl;
            return;
        }
        
        // Create topic: market.<exchange>.<symbol>
        std::string topic = "market." + exchange_name + "." + orderbook.symbol();
        
        // Publish to ZMQ
        publisher.publish(topic, serialized_orderbook);
        
        std::cout << "[MARKET_DATA] " << exchange_name << " " << orderbook.symbol() 
                  << " bids=" << orderbook.bids_size() << " asks=" << orderbook.asks_size() << std::endl;
    });
    
    // Set up trade callback
    subscriber->set_trade_callback([&publisher, &exchange_name](const proto::Trade& trade) {
        // Serialize trade
        std::string serialized_trade;
        if (!trade.SerializeToString(&serialized_trade)) {
            std::cerr << "[MARKET_SERVER] Failed to serialize trade" << std::endl;
            return;
        }
        
        // Create topic: trades.<exchange>.<symbol>
        std::string topic = "trades." + exchange_name + "." + trade.symbol();
        
        // Publish to ZMQ
        publisher.publish(topic, serialized_trade);
        
        std::cout << "[TRADE] " << exchange_name << " " << trade.symbol() 
                  << " price=" << trade.price() << " qty=" << trade.qty() << std::endl;
    });
    
    // Connect to exchange
    if (!subscriber->connect()) {
        std::cerr << "Failed to connect to exchange: " << exchange_name << std::endl;
        return 1;
    }
    
    // Subscribe to symbols (parse comma-separated list)
    std::vector<std::string> symbols;
    std::stringstream ss(symbols_str);
    std::string symbol;
    while (std::getline(ss, symbol, ',')) {
        // Trim whitespace
        symbol.erase(0, symbol.find_first_not_of(" \t"));
        symbol.erase(symbol.find_last_not_of(" \t") + 1);
        
        if (!symbol.empty()) {
            // Subscribe to orderbook (top 10 levels, 100ms updates)
            subscriber->subscribe_orderbook(symbol, 10, 100);
            // Subscribe to trades
            subscriber->subscribe_trades(symbol);
            
            std::cout << "[MARKET_SERVER] Subscribed to " << symbol << std::endl;
        }
    }
    
    std::cout << "Market server running. Press Ctrl+C to stop." << std::endl;
    
    // Keep running until signal received
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check connection health
        if (!subscriber->is_connected()) {
            std::cerr << "[MARKET_SERVER] Connection lost, attempting to reconnect..." << std::endl;
            subscriber->disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (!subscriber->connect()) {
                std::cerr << "[MARKET_SERVER] Reconnection failed" << std::endl;
            }
        }
    }
    
    // Clean shutdown
    subscriber->disconnect();
    std::cout << "Market server stopped." << std::endl;
    return 0;
}
