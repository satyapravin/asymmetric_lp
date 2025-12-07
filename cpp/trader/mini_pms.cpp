#include "mini_pms.hpp"
#include "../utils/logging/log_helper.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace trader {

MiniPMS::MiniPMS() : running_(false) {
    statistics_.reset();
}

void MiniPMS::start() {
    if (running_.load()) {
        return;
    }
    
    LOG_INFO_COMP("MINI_PMS", "Starting Mini Position Management System");
    running_.store(true);
    
    // Clear any existing positions
    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        positions_.clear();
    }
    
    statistics_.reset();
    LOG_INFO_COMP("MINI_PMS", "Mini PMS started successfully");
}

void MiniPMS::stop() {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO_COMP("MINI_PMS", "Stopping Mini Position Management System");
    running_.store(false);
    
    // Clear callback
    clear_position_update_callback();
    
    LOG_INFO_COMP("MINI_PMS", "Mini PMS stopped");
}

void MiniPMS::update_position(const proto::PositionUpdate& position_update) {
    if (!running_.load()) {
        return;
    }
    
    // Extract data from protobuf
    std::string exchange = position_update.exch();
    std::string symbol = position_update.symbol();
    double qty = position_update.qty();
    double avg_price = position_update.avg_price();
    double unrealized_pnl = 0.0; // Not available in proto, will be calculated separately
    
    update_position(exchange, symbol, qty, avg_price, unrealized_pnl);
}

void MiniPMS::update_account_balance(const proto::AccountBalanceUpdate& balance_update) {
    if (!running_.load()) {
        return;
    }
    
    // Process each balance in the update
    for (int i = 0; i < balance_update.balances_size(); ++i) {
        const auto& balance = balance_update.balances(i);
        
        std::string exchange = balance.exch();
        std::string instrument = balance.instrument();
        double total_balance = balance.balance();
        double available = balance.available();
        double locked = balance.locked();
        
        update_account_balance(exchange, instrument, total_balance, available, locked);
    }
}

void MiniPMS::update_account_balance(const std::string& exchange, const std::string& instrument, 
                                    double balance, double available, double locked) {
    if (!running_.load()) {
        return;
    }
    
    AccountBalanceInfo balance_info_copy;
    
    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        
        // Check if balance exists
        bool balance_exists = (account_balances_.find(exchange) != account_balances_.end() && 
                              account_balances_[exchange].find(instrument) != account_balances_[exchange].end());
        
        // Create or update balance
        AccountBalanceInfo balance_info(exchange, instrument, balance, available, locked);
        balance_info.last_update_time = get_current_time_string();
        
        if (!balance_exists) {
            // Create new balance
            account_balances_[exchange][instrument] = balance_info;
            statistics_.position_creates.fetch_add(1);
            
            std::string log_msg = "Created balance: " + exchange + ":" + instrument + 
                                  " balance=" + std::to_string(balance) + 
                                  " available=" + std::to_string(available) + 
                                  " locked=" + std::to_string(locked);
            LOG_DEBUG_COMP("MINI_PMS", log_msg);
            
            balance_info_copy = balance_info;
        } else {
            // Update existing balance
            AccountBalanceInfo& existing = account_balances_[exchange][instrument];
            existing.balance = balance;
            existing.available = available;
            existing.locked = locked;
            existing.last_update_time = get_current_time_string();
            
            statistics_.position_updates.fetch_add(1);
            
            std::string log_msg = "Updated balance: " + exchange + ":" + instrument + 
                                  " balance=" + std::to_string(balance) + 
                                  " available=" + std::to_string(available) + 
                                  " locked=" + std::to_string(locked);
            LOG_DEBUG_COMP("MINI_PMS", log_msg);
            
            balance_info_copy = existing;
        }
        
        statistics_.total_updates.fetch_add(1);
    }  // Release positions_mutex_ BEFORE callback to prevent deadlock
    
    // Notify callback AFTER releasing lock (prevents deadlock if callback queries balances)
    notify_account_balance_update(balance_info_copy);
}

void MiniPMS::update_position(const std::string& exchange, const std::string& symbol, 
                              double qty, double avg_price, double unrealized_pnl) {
    if (!running_.load()) {
        return;
    }
    
    PositionInfo position_copy;
    
    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        
        // Check if position exists
        bool position_exists = (positions_.find(exchange) != positions_.end() && 
                               positions_[exchange].find(symbol) != positions_[exchange].end());
        
        // Create or update position
        PositionInfo position(exchange, symbol, qty, avg_price, unrealized_pnl);
        
        if (!position_exists) {
            // Create new position
            positions_[exchange][symbol] = position;
            statistics_.position_creates.fetch_add(1);
            
            std::string log_msg = "Created position: " + exchange + ":" + symbol + 
                                  " qty=" + std::to_string(qty) + 
                                  " price=" + std::to_string(avg_price);
            LOG_DEBUG_COMP("MINI_PMS", log_msg);
            
            position_copy = position;
        } else {
            // Update existing position
            PositionInfo& existing = positions_[exchange][symbol];
            existing.qty = qty;
            existing.avg_price = avg_price;
            existing.unrealized_pnl = unrealized_pnl;
            existing.last_update_time = get_current_time_string();
            
            statistics_.position_updates.fetch_add(1);
            
            std::string log_msg = "Updated position: " + exchange + ":" + symbol + 
                                  " qty=" + std::to_string(qty) + 
                                  " price=" + std::to_string(avg_price) + 
                                  " unrealized_pnl=" + std::to_string(unrealized_pnl);
            LOG_DEBUG_COMP("MINI_PMS", log_msg);
            
            position_copy = existing;
        }
        
        statistics_.total_updates.fetch_add(1);
    }  // Release positions_mutex_ BEFORE callback to prevent deadlock
    
    // Notify callback AFTER releasing lock (prevents deadlock if callback queries positions)
    notify_position_update(position_copy);
}

std::optional<PositionInfo> MiniPMS::get_position(const std::string& exchange, 
                                                 const std::string& symbol) const {
    if (!running_.load()) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    auto exchange_it = positions_.find(exchange);
    if (exchange_it == positions_.end()) {
        return std::nullopt;
    }
    
    auto symbol_it = exchange_it->second.find(symbol);
    if (symbol_it == exchange_it->second.end()) {
        return std::nullopt;
    }
    
    return symbol_it->second;
}

std::vector<PositionInfo> MiniPMS::get_all_positions() const {
    std::vector<PositionInfo> all_positions;
    
    if (!running_.load()) {
        return all_positions;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    for (const auto& exchange_pair : positions_) {
        for (const auto& symbol_pair : exchange_pair.second) {
            all_positions.push_back(symbol_pair.second);
        }
    }
    
    return all_positions;
}

std::vector<PositionInfo> MiniPMS::get_positions_by_exchange(const std::string& exchange) const {
    std::vector<PositionInfo> exchange_positions;
    
    if (!running_.load()) {
        return exchange_positions;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    auto exchange_it = positions_.find(exchange);
    if (exchange_it != positions_.end()) {
        for (const auto& symbol_pair : exchange_it->second) {
            exchange_positions.push_back(symbol_pair.second);
        }
    }
    
    return exchange_positions;
}

std::vector<PositionInfo> MiniPMS::get_positions_by_symbol(const std::string& symbol) const {
    std::vector<PositionInfo> symbol_positions;
    
    if (!running_.load()) {
        return symbol_positions;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    for (const auto& exchange_pair : positions_) {
        auto symbol_it = exchange_pair.second.find(symbol);
        if (symbol_it != exchange_pair.second.end()) {
            symbol_positions.push_back(symbol_it->second);
        }
    }
    
    return symbol_positions;
}

// Account balance queries
std::optional<AccountBalanceInfo> MiniPMS::get_account_balance(const std::string& exchange, 
                                                              const std::string& instrument) const {
    if (!running_.load()) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    auto exchange_it = account_balances_.find(exchange);
    if (exchange_it == account_balances_.end()) {
        return std::nullopt;
    }
    
    auto instrument_it = exchange_it->second.find(instrument);
    if (instrument_it == exchange_it->second.end()) {
        return std::nullopt;
    }
    
    return instrument_it->second;
}

std::vector<AccountBalanceInfo> MiniPMS::get_all_account_balances() const {
    std::vector<AccountBalanceInfo> all_balances;
    
    if (!running_.load()) {
        return all_balances;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    for (const auto& exchange_pair : account_balances_) {
        for (const auto& instrument_pair : exchange_pair.second) {
            all_balances.push_back(instrument_pair.second);
        }
    }
    
    return all_balances;
}

std::vector<AccountBalanceInfo> MiniPMS::get_account_balances_by_exchange(const std::string& exchange) const {
    std::vector<AccountBalanceInfo> exchange_balances;
    
    if (!running_.load()) {
        return exchange_balances;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    auto exchange_it = account_balances_.find(exchange);
    if (exchange_it != account_balances_.end()) {
        for (const auto& instrument_pair : exchange_it->second) {
            exchange_balances.push_back(instrument_pair.second);
        }
    }
    
    return exchange_balances;
}

std::vector<AccountBalanceInfo> MiniPMS::get_account_balances_by_instrument(const std::string& instrument) const {
    std::vector<AccountBalanceInfo> instrument_balances;
    
    if (!running_.load()) {
        return instrument_balances;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    for (const auto& exchange_pair : account_balances_) {
        auto instrument_it = exchange_pair.second.find(instrument);
        if (instrument_it != exchange_pair.second.end()) {
            instrument_balances.push_back(instrument_it->second);
        }
    }
    
    return instrument_balances;
}

double MiniPMS::get_total_unrealized_pnl() const {
    if (!running_.load()) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    double total_unrealized = 0.0;
    for (const auto& exchange_pair : positions_) {
        for (const auto& symbol_pair : exchange_pair.second) {
            total_unrealized += symbol_pair.second.unrealized_pnl;
        }
    }
    
    return total_unrealized;
}

double MiniPMS::get_total_realized_pnl() const {
    if (!running_.load()) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    double total_realized = 0.0;
    for (const auto& exchange_pair : positions_) {
        for (const auto& symbol_pair : exchange_pair.second) {
            total_realized += symbol_pair.second.realized_pnl;
        }
    }
    
    return total_realized;
}

double MiniPMS::get_net_position_value() const {
    if (!running_.load()) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    double net_value = 0.0;
    for (const auto& exchange_pair : positions_) {
        for (const auto& symbol_pair : exchange_pair.second) {
            const PositionInfo& pos = symbol_pair.second;
            net_value += pos.qty * pos.avg_price;
        }
    }
    
    return net_value;
}

size_t MiniPMS::get_position_count() const {
    if (!running_.load()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(positions_mutex_);
    
    size_t count = 0;
    for (const auto& exchange_pair : positions_) {
        count += exchange_pair.second.size();
    }
    
    return count;
}

void MiniPMS::set_position_update_callback(PositionUpdateCallback callback) {
    position_callback_ = callback;
    LOG_INFO_COMP("MINI_PMS", "Position update callback set");
}

void MiniPMS::clear_position_update_callback() {
    position_callback_ = nullptr;
    LOG_INFO_COMP("MINI_PMS", "Position update callback cleared");
}

void MiniPMS::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    account_balance_callback_ = callback;
    LOG_INFO_COMP("MINI_PMS", "Account balance update callback set");
}

void MiniPMS::clear_account_balance_update_callback() {
    account_balance_callback_ = nullptr;
    LOG_INFO_COMP("MINI_PMS", "Account balance update callback cleared");
}

void MiniPMS::notify_position_update(const PositionInfo& position) {
    if (position_callback_) {
        try {
            position_callback_(position);
            statistics_.callback_notifications.fetch_add(1);
        } catch (const std::exception& e) {
            LOG_ERROR_COMP("MINI_PMS", "Error in position update callback: " + std::string(e.what()));
        }
    }
}

void MiniPMS::notify_account_balance_update(const AccountBalanceInfo& balance) {
    if (account_balance_callback_) {
        try {
            account_balance_callback_(balance);
            statistics_.callback_notifications.fetch_add(1);
        } catch (const std::exception& e) {
            LOG_ERROR_COMP("MINI_PMS", "Error in account balance update callback: " + std::string(e.what()));
        }
    }
}

std::string MiniPMS::generate_position_key(const std::string& exchange, 
                                          const std::string& symbol) const {
    return exchange + ":" + symbol;
}

std::string MiniPMS::generate_balance_key(const std::string& exchange, 
                                         const std::string& instrument) const {
    return exchange + ":" + instrument;
}

void MiniPMS::log_position_event(const std::string& event, const PositionInfo& position) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    std::string log_msg = ss.str() + " " + event + " " + position.exchange + ":" + position.symbol +
                          " qty=" + std::to_string(position.qty) + 
                          " price=" + std::to_string(position.avg_price);
    LOG_DEBUG_COMP("MINI_PMS", log_msg);
}

void MiniPMS::log_balance_event(const std::string& event, const AccountBalanceInfo& balance) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    std::string log_msg = ss.str() + " " + event + " " + balance.exchange + ":" + balance.instrument +
                          " balance=" + std::to_string(balance.balance) + 
                          " available=" + std::to_string(balance.available) + 
                          " locked=" + std::to_string(balance.locked);
    LOG_INFO_COMP("MINI_PMS", log_msg);
}

std::string MiniPMS::get_current_time_string() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace trader
