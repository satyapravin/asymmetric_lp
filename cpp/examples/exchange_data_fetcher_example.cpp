#include <iostream>
#include <memory>
#include "exchanges/binance/binance_oms.hpp"
#include "exchanges/binance/binance_data_fetcher.hpp"

int main() {
    std::cout << "=== Exchange-Specific Data Fetcher Example ===" << std::endl;
    
    // Create Binance configuration
    binance::BinanceConfig config;
    config.api_key = "your_api_key";
    config.api_secret = "your_api_secret";
    config.base_url = "https://api.binance.com";
    config.testnet_mode = true;
    
    // Create Binance OMS with built-in data fetcher
    auto binance_oms = std::make_unique<binance::BinanceOMS>(config);
    
    // Connect to Binance
    if (binance_oms->connect().is_success()) {
        std::cout << "Connected to Binance successfully" << std::endl;
        
        // Fetch data using exchange-specific methods
        std::cout << "\n=== Fetching Exchange Data ===" << std::endl;
        
        // Get active orders
        auto active_orders = binance_oms->get_active_orders();
        std::cout << "Active orders: " << active_orders.size() << std::endl;
        for (const auto& order : active_orders) {
            std::cout << "  Order: " << order.cl_ord_id << " " << order.symbol 
                      << " " << order.side << " " << order.qty << "@" << order.price << std::endl;
        }
        
        // Get positions
        auto positions = binance_oms->get_positions();
        std::cout << "Positions: " << positions.size() << std::endl;
        for (const auto& position : positions) {
            std::cout << "  Position: " << position.symbol << " " << position.qty 
                      << " avg_price=" << position.avg_price << " pnl=" << position.unrealized_pnl << std::endl;
        }
        
        // Get balances
        auto balances = binance_oms->get_balances();
        std::cout << "Balances: " << balances.size() << std::endl;
        for (const auto& balance : balances) {
            std::cout << "  Balance: " << balance.asset << " free=" << balance.free_balance 
                      << " locked=" << balance.locked_balance << std::endl;
        }
        
        // Get trade history
        auto trades = binance_oms->get_trade_history("BTCUSDT", 0, 0);
        std::cout << "Recent trades: " << trades.size() << std::endl;
        for (const auto& trade : trades) {
            std::cout << "  Trade: " << trade.cl_ord_id << " " << trade.symbol 
                      << " " << trade.side << " " << trade.qty << "@" << trade.price << std::endl;
        }
        
        // Disconnect
        binance_oms->disconnect();
        
    } else {
        std::cout << "Failed to connect to Binance" << std::endl;
    }
    
    std::cout << "\n=== Example completed ===" << std::endl;
    return 0;
}
