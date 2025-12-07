#pragma once
#include "exchange_symbol_info.hpp"
#include <map>
#include <mutex>
#include <memory>
#include <string>

/**
 * Exchange Symbol Registry
 * 
 * Manages exchange-specific symbol information for order validation.
 * Loads symbol info from configuration files and provides validation utilities.
 */
class ExchangeSymbolRegistry {
public:
    static ExchangeSymbolRegistry& get_instance() {
        static ExchangeSymbolRegistry instance;
        return instance;
    }
    
    // Load symbol info from config file
    bool load_from_config(const std::string& config_file_path);
    
    // Get symbol info (returns empty/invalid info if not found)
    ExchangeSymbolInfo get_symbol_info(const std::string& exchange, 
                                      const std::string& symbol) const;
    
    // Check if symbol info exists
    bool has_symbol_info(const std::string& exchange, 
                        const std::string& symbol) const;
    
    // Round price to tick size
    double round_to_tick(double price, const ExchangeSymbolInfo& info) const;
    
    // Round quantity to step size
    double round_to_step(double qty, const ExchangeSymbolInfo& info) const;
    
    // Validate order parameters against symbol info
    bool validate_order_params(const ExchangeSymbolInfo& info, 
                               double qty, double price) const;
    
    // Validate and round order parameters
    // Returns true if valid, modifies qty and price in place
    bool validate_and_round(const std::string& exchange,
                           const std::string& symbol,
                           double& qty, double& price) const;
    
    // Validate only (assumes values are already rounded)
    // Returns true if valid, does not modify qty and price
    bool validate_only(const std::string& exchange,
                      const std::string& symbol,
                      double qty, double price) const;
    
private:
    ExchangeSymbolRegistry() = default;
    ~ExchangeSymbolRegistry() = default;
    ExchangeSymbolRegistry(const ExchangeSymbolRegistry&) = delete;
    ExchangeSymbolRegistry& operator=(const ExchangeSymbolRegistry&) = delete;
    
    // Key format: "exchange:symbol"
    std::map<std::string, ExchangeSymbolInfo> symbol_info_map_;
    mutable std::mutex mutex_;
    
    std::string make_key(const std::string& exchange, const std::string& symbol) const;
};

