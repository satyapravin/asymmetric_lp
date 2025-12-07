#include "exchange_symbol_registry.hpp"
#include "../config/config.hpp"
#include "../config/process_config_manager.hpp"
#include "../logging/log_helper.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

ExchangeSymbolInfo ExchangeSymbolRegistry::get_symbol_info(
    const std::string& exchange, const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(exchange, symbol);
    auto it = symbol_info_map_.find(key);
    if (it != symbol_info_map_.end()) {
        return it->second;
    }
    
    // Return invalid info if not found
    ExchangeSymbolInfo empty_info;
    empty_info.symbol = symbol;
    empty_info.exchange = exchange;
    empty_info.is_valid = false;
    return empty_info;
}

bool ExchangeSymbolRegistry::has_symbol_info(
    const std::string& exchange, const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = make_key(exchange, symbol);
    return symbol_info_map_.find(key) != symbol_info_map_.end();
}

double ExchangeSymbolRegistry::round_to_tick(
    double price, const ExchangeSymbolInfo& info) const {
    if (!info.is_valid || info.tick_size <= 0.0) {
        return price; // Cannot round without valid tick size
    }
    
    // Round down to nearest tick
    double rounded = std::floor(price / info.tick_size) * info.tick_size;
    
    // Round to precision to avoid floating point issues
    double multiplier = std::pow(10.0, info.price_precision);
    return std::round(rounded * multiplier) / multiplier;
}

double ExchangeSymbolRegistry::round_to_step(
    double qty, const ExchangeSymbolInfo& info) const {
    if (!info.is_valid || info.step_size <= 0.0) {
        return qty; // Cannot round without valid step size
    }
    
    // Round down to nearest step
    double rounded = std::floor(qty / info.step_size) * info.step_size;
    
    // Round to precision to avoid floating point issues
    double multiplier = std::pow(10.0, info.qty_precision);
    return std::round(rounded * multiplier) / multiplier;
}

bool ExchangeSymbolRegistry::validate_order_params(
    const ExchangeSymbolInfo& info, double qty, double price) const {
    if (!info.is_valid) {
        return false; // Cannot validate without symbol info
    }
    
    // Validate quantity
    if (qty <= 0.0) {
        return false;
    }
    
    if (info.min_order_size > 0.0 && qty < info.min_order_size) {
        return false;
    }
    
    if (info.max_order_size > 0.0 && qty > info.max_order_size) {
        return false;
    }
    
    // Validate price
    if (price <= 0.0) {
        return false;
    }
    
    // Validate tick size alignment
    if (info.tick_size > 0.0) {
        double remainder = std::fmod(price, info.tick_size);
        // Allow small floating point errors
        if (remainder > 1e-10 && remainder < (info.tick_size - 1e-10)) {
            return false;
        }
    }
    
    // Validate step size alignment
    if (info.step_size > 0.0) {
        double remainder = std::fmod(qty, info.step_size);
        // Allow small floating point errors
        if (remainder > 1e-10 && remainder < (info.step_size - 1e-10)) {
            return false;
        }
    }
    
    return true;
}

bool ExchangeSymbolRegistry::validate_and_round(
    const std::string& exchange, const std::string& symbol,
    double& qty, double& price) const {
    ExchangeSymbolInfo info = get_symbol_info(exchange, symbol);
    
    if (!info.is_valid) {
        // If no symbol info, allow order but log warning
        LOG_WARN_COMP("SYMBOL_REGISTRY", 
                     "No symbol info for " + exchange + ":" + symbol + 
                     " - skipping validation");
        return true; // Allow order if no config
    }
    
    // Round quantities and prices
    double original_qty = qty;
    double original_price = price;
    
    qty = round_to_step(qty, info);
    price = round_to_tick(price, info);
    
    // Validate after rounding
    if (!validate_order_params(info, qty, price)) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Order validation failed for " + exchange + ":" + symbol +
                      " qty=" + std::to_string(original_qty) + "->" + std::to_string(qty) +
                      " price=" + std::to_string(original_price) + "->" + std::to_string(price));
        return false;
    }
    
    // Log if rounding occurred
    if (std::abs(original_qty - qty) > 1e-10 || std::abs(original_price - price) > 1e-10) {
        LOG_DEBUG_COMP("SYMBOL_REGISTRY",
                      "Rounded order params for " + exchange + ":" + symbol +
                      " qty: " + std::to_string(original_qty) + " -> " + std::to_string(qty) +
                      " price: " + std::to_string(original_price) + " -> " + std::to_string(price));
    }
    
    return true;
}

bool ExchangeSymbolRegistry::validate_only(const std::string& exchange,
                                           const std::string& symbol,
                                           double qty, double price) const {
    ExchangeSymbolInfo info = get_symbol_info(exchange, symbol);
    
    if (!info.is_valid) {
        // If no symbol info, allow order but log warning
        LOG_WARN_COMP("SYMBOL_REGISTRY", 
                     "No symbol info for " + exchange + ":" + symbol + 
                     " - skipping validation");
        return true; // Allow order if no config
    }
    
    // Validate against exchange-specific constraints (assume already rounded)
    if (!validate_order_params(info, qty, price)) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Order validation failed for " + exchange + ":" + symbol +
                      " qty=" + std::to_string(qty) + " price=" + std::to_string(price));
        return false;
    }
    
    return true;
}

bool ExchangeSymbolRegistry::load_from_config(const std::string& config_file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO_COMP("SYMBOL_REGISTRY", "Loading symbol info from: " + config_file_path);
    
    config::ProcessConfigManager config_manager;
    if (!config_manager.load_config(config_file_path)) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY", "Failed to load config file: " + config_file_path);
        return false;
    }
    
    // Get all sections (each section is a symbol)
    // Format: [EXCHANGE:SYMBOL] or [SYMBOL] (default exchange)
    std::vector<std::string> sections = config_manager.get_sections();
    
    int loaded_count = 0;
    for (const auto& section : sections) {
        // Parse section name - could be "EXCHANGE:SYMBOL" or just "SYMBOL"
        std::string exchange, symbol;
        size_t colon_pos = section.find(':');
        
        if (colon_pos != std::string::npos) {
            exchange = section.substr(0, colon_pos);
            symbol = section.substr(colon_pos + 1);
        } else {
            // Default exchange if not specified
            exchange = config_manager.get_string(section, "exchange", "DEFAULT");
            symbol = section;
        }
        
        // Load symbol parameters
        double tick_size = config_manager.get_double(section, "tick_size", 0.0);
        double step_size = config_manager.get_double(section, "step_size", 0.0);
        double min_order_size = config_manager.get_double(section, "min_order_size", 0.0);
        double max_order_size = config_manager.get_double(section, "max_order_size", 0.0);
        int price_precision = config_manager.get_int(section, "price_precision", 8);
        int qty_precision = config_manager.get_int(section, "qty_precision", 8);
        
        // Load contract specifications (optional, for perpetuals)
        double contract_size = config_manager.get_double(section, "contract_size", 0.0);
        std::string contract_size_denomination = config_manager.get_string(section, "contract_size_denomination", "");
        
        // Validate required parameters
        if (tick_size <= 0.0 || step_size <= 0.0) {
            LOG_WARN_COMP("SYMBOL_REGISTRY",
                         "Skipping " + exchange + ":" + symbol + 
                         " - invalid tick_size or step_size");
            continue;
        }
        
        // Create symbol info
        ExchangeSymbolInfo info(symbol, exchange, tick_size, step_size,
                                min_order_size, max_order_size,
                                price_precision, qty_precision);
        
        // Set contract specifications if provided
        info.contract_size = contract_size;
        info.contract_size_denomination = contract_size_denomination;
        
        std::string key = make_key(exchange, symbol);
        symbol_info_map_[key] = info;
        loaded_count++;
        
        std::string log_msg = "Loaded " + exchange + ":" + symbol +
                             " tick=" + std::to_string(tick_size) +
                             " step=" + std::to_string(step_size) +
                             " min=" + std::to_string(min_order_size) +
                             " max=" + std::to_string(max_order_size);
        if (contract_size > 0.0 && !contract_size_denomination.empty()) {
            log_msg += " contract_size=" + std::to_string(contract_size) +
                      " " + contract_size_denomination;
        }
        LOG_DEBUG_COMP("SYMBOL_REGISTRY", log_msg);
    }
    
    LOG_INFO_COMP("SYMBOL_REGISTRY", 
                 "Loaded " + std::to_string(loaded_count) + " symbol configurations");
    
    return loaded_count > 0;
}

std::string ExchangeSymbolRegistry::make_key(
    const std::string& exchange, const std::string& symbol) const {
    return exchange + ":" + symbol;
}

double ExchangeSymbolRegistry::token_qty_to_contracts(
    const std::string& exchange,
    const std::string& symbol,
    double token_qty,
    double price) const {
    
    ExchangeSymbolInfo info = get_symbol_info(exchange, symbol);
    
    if (!info.is_valid) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert token_qty to contracts: no symbol info for " +
                      exchange + ":" + symbol);
        return 0.0;
    }
    
    if (info.contract_size <= 0.0 || info.contract_size_denomination.empty()) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert token_qty to contracts: contract_size not configured for " +
                      exchange + ":" + symbol);
        return 0.0;
    }
    
    if (price <= 0.0) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert token_qty to contracts: invalid price " +
                      std::to_string(price) + " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    // Calculate notional USD value of the token quantity
    double notional_usd = token_qty * price;
    
    // Calculate contract size in USD
    double contract_size_usd = 0.0;
    
    if (info.contract_size_denomination == "BTC" || 
        info.contract_size_denomination == "ETH" ||
        info.contract_size_denomination == "SOL") {
        // Contract size is in base token (BTC, ETH, etc.)
        // Need to convert to USD using the same price
        contract_size_usd = info.contract_size * price;
    } else if (info.contract_size_denomination == "USDC" ||
               info.contract_size_denomination == "USDT" ||
               info.contract_size_denomination == "USD") {
        // Contract size is already in USD terms
        contract_size_usd = info.contract_size;
    } else {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Unknown contract_size_denomination: " + info.contract_size_denomination +
                      " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    if (contract_size_usd <= 0.0) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Invalid contract_size_usd calculated: " +
                      std::to_string(contract_size_usd) + " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    // Calculate number of contracts
    double contracts = notional_usd / contract_size_usd;
    
    LOG_DEBUG_COMP("SYMBOL_REGISTRY",
                  "token_qty_to_contracts: " + exchange + ":" + symbol +
                  " token_qty=" + std::to_string(token_qty) +
                  " price=" + std::to_string(price) +
                  " notional_usd=" + std::to_string(notional_usd) +
                  " contract_size=" + std::to_string(info.contract_size) +
                  " " + info.contract_size_denomination +
                  " contract_size_usd=" + std::to_string(contract_size_usd) +
                  " contracts=" + std::to_string(contracts));
    
    return contracts;
}

double ExchangeSymbolRegistry::contracts_to_token_qty(
    const std::string& exchange,
    const std::string& symbol,
    double contracts,
    double price) const {
    
    ExchangeSymbolInfo info = get_symbol_info(exchange, symbol);
    
    if (!info.is_valid) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert contracts to token_qty: no symbol info for " +
                      exchange + ":" + symbol);
        return 0.0;
    }
    
    if (info.contract_size <= 0.0 || info.contract_size_denomination.empty()) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert contracts to token_qty: contract_size not configured for " +
                      exchange + ":" + symbol);
        return 0.0;
    }
    
    if (price <= 0.0) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Cannot convert contracts to token_qty: invalid price " +
                      std::to_string(price) + " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    // Calculate contract size in USD
    double contract_size_usd = 0.0;
    
    if (info.contract_size_denomination == "BTC" || 
        info.contract_size_denomination == "ETH" ||
        info.contract_size_denomination == "SOL") {
        // Contract size is in base token (BTC, ETH, etc.)
        contract_size_usd = info.contract_size * price;
    } else if (info.contract_size_denomination == "USDC" ||
               info.contract_size_denomination == "USDT" ||
               info.contract_size_denomination == "USD") {
        // Contract size is already in USD terms
        contract_size_usd = info.contract_size;
    } else {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Unknown contract_size_denomination: " + info.contract_size_denomination +
                      " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    if (contract_size_usd <= 0.0) {
        LOG_ERROR_COMP("SYMBOL_REGISTRY",
                      "Invalid contract_size_usd calculated: " +
                      std::to_string(contract_size_usd) + " for " + exchange + ":" + symbol);
        return 0.0;
    }
    
    // Calculate notional USD value
    double notional_usd = contracts * contract_size_usd;
    
    // Convert notional USD to token quantity
    double token_qty = notional_usd / price;
    
    LOG_DEBUG_COMP("SYMBOL_REGISTRY",
                  "contracts_to_token_qty: " + exchange + ":" + symbol +
                  " contracts=" + std::to_string(contracts) +
                  " contract_size=" + std::to_string(info.contract_size) +
                  " " + info.contract_size_denomination +
                  " contract_size_usd=" + std::to_string(contract_size_usd) +
                  " notional_usd=" + std::to_string(notional_usd) +
                  " price=" + std::to_string(price) +
                  " token_qty=" + std::to_string(token_qty));
    
    return token_qty;
}

