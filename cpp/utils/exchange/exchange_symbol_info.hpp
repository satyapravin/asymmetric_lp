#pragma once
#include <string>
#include <map>

/**
 * Exchange-specific symbol information
 * 
 * Contains validation rules and constraints for trading symbols
 * on specific exchanges.
 */
struct ExchangeSymbolInfo {
    std::string symbol;
    std::string exchange;
    
    // Price constraints
    double tick_size{0.0};        // Minimum price increment (e.g., 0.01)
    int price_precision{8};       // Decimal places for price
    
    // Quantity constraints
    double step_size{0.0};        // Minimum quantity increment (e.g., 0.001)
    int qty_precision{8};         // Decimal places for quantity
    
    // Order size constraints
    double min_order_size{0.0};   // Minimum order size
    double max_order_size{0.0};   // Maximum order size (0 = unlimited)
    
    // Contract specifications (for perpetuals)
    double contract_size{0.0};              // Contract size (e.g., 0.01 BTC, 1.0 USDC)
    std::string contract_size_denomination; // Denomination currency (e.g., "BTC", "USDC", "USDT")
    
    // Validation flags
    bool is_valid{false};         // True if symbol info is loaded and valid
    
    ExchangeSymbolInfo() = default;
    
    ExchangeSymbolInfo(const std::string& sym, const std::string& exch,
                      double tick, double step, double min_size, double max_size = 0.0,
                      int price_prec = 8, int qty_prec = 8)
        : symbol(sym), exchange(exch), tick_size(tick), step_size(step),
          min_order_size(min_size), max_order_size(max_size),
          price_precision(price_prec), qty_precision(qty_prec), is_valid(true) {}
};

