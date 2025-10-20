#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <optional>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../proto/position.pb.h"
#include "../proto/acc_balance.pb.h"

namespace trader {

// Position information structure
struct PositionInfo {
    std::string exchange;
    std::string symbol;
    double qty;
    double avg_price;
    double unrealized_pnl;
    double realized_pnl;
    std::string last_update_time;
    
    PositionInfo() : qty(0.0), avg_price(0.0), unrealized_pnl(0.0), realized_pnl(0.0) {}
    
    PositionInfo(const std::string& exch, const std::string& sym, double quantity, 
                 double price, double unreal_pnl = 0.0, double real_pnl = 0.0)
        : exchange(exch), symbol(sym), qty(quantity), avg_price(price), 
          unrealized_pnl(unreal_pnl), realized_pnl(real_pnl) {}
};

// Account balance information structure
struct AccountBalanceInfo {
    std::string exchange;
    std::string instrument;
    double balance;
    double available;
    double locked;
    std::string last_update_time;
    
    AccountBalanceInfo() : balance(0.0), available(0.0), locked(0.0) {}
    
    AccountBalanceInfo(const std::string& exch, const std::string& instr, 
                      double bal, double avail, double lock)
        : exchange(exch), instrument(instr), balance(bal), available(avail), locked(lock) {}
};

// Position update callback type
using PositionUpdateCallback = std::function<void(const PositionInfo&)>;

// Account balance update callback type
using AccountBalanceUpdateCallback = std::function<void(const AccountBalanceInfo&)>;

// Mini PMS class for position management
class MiniPMS {
public:
    MiniPMS();
    ~MiniPMS() = default;
    
    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Position management
    void update_position(const proto::PositionUpdate& position_update);
    void update_position(const std::string& exchange, const std::string& symbol, 
                        double qty, double avg_price, double unrealized_pnl = 0.0);
    
    // Account balance management
    void update_account_balance(const proto::AccountBalanceUpdate& balance_update);
    void update_account_balance(const std::string& exchange, const std::string& instrument, 
                               double balance, double available, double locked);
    
    // Position queries
    std::optional<PositionInfo> get_position(const std::string& exchange, 
                                            const std::string& symbol) const;
    std::vector<PositionInfo> get_all_positions() const;
    std::vector<PositionInfo> get_positions_by_exchange(const std::string& exchange) const;
    std::vector<PositionInfo> get_positions_by_symbol(const std::string& symbol) const;
    
    // Account balance queries
    std::optional<AccountBalanceInfo> get_account_balance(const std::string& exchange, 
                                                         const std::string& instrument) const;
    std::vector<AccountBalanceInfo> get_all_account_balances() const;
    std::vector<AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const;
    std::vector<AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const;
    
    // Position statistics
    double get_total_unrealized_pnl() const;
    double get_total_realized_pnl() const;
    double get_net_position_value() const;
    size_t get_position_count() const;
    
    // Callback management
    void set_position_update_callback(PositionUpdateCallback callback);
    void clear_position_update_callback();
    void set_account_balance_update_callback(AccountBalanceUpdateCallback callback);
    void clear_account_balance_update_callback();
    
    // Statistics and monitoring
    struct Statistics {
        std::atomic<size_t> total_updates{0};
        std::atomic<size_t> position_creates{0};
        std::atomic<size_t> position_updates{0};
        std::atomic<size_t> position_deletes{0};
        std::atomic<size_t> callback_notifications{0};
        
        void reset() {
            total_updates.store(0);
            position_creates.store(0);
            position_updates.store(0);
            position_deletes.store(0);
            callback_notifications.store(0);
        }
    };
    
    const Statistics& get_statistics() const { return statistics_; }
    void reset_statistics() { statistics_.reset(); }
    
private:
    // Internal state
    std::atomic<bool> running_;
    mutable std::mutex positions_mutex_;
    
    // Position storage: exchange -> symbol -> PositionInfo
    std::unordered_map<std::string, 
        std::unordered_map<std::string, PositionInfo>> positions_;
    
    // Account balance storage: exchange -> instrument -> AccountBalanceInfo
    std::unordered_map<std::string, 
        std::unordered_map<std::string, AccountBalanceInfo>> account_balances_;
    
    // Callbacks
    PositionUpdateCallback position_callback_;
    AccountBalanceUpdateCallback account_balance_callback_;
    
    // Statistics
    Statistics statistics_;
    
    // Internal methods
    void notify_position_update(const PositionInfo& position);
    void notify_account_balance_update(const AccountBalanceInfo& balance);
    std::string generate_position_key(const std::string& exchange, 
                                     const std::string& symbol) const;
    std::string generate_balance_key(const std::string& exchange, 
                                    const std::string& instrument) const;
    void log_position_event(const std::string& event, const PositionInfo& position);
    void log_balance_event(const std::string& event, const AccountBalanceInfo& balance);
    std::string get_current_time_string() const;
};

} // namespace trader
