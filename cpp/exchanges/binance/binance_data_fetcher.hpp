#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>

namespace binance {

// Binance-specific data structures
struct BinanceOrder {
    std::string cl_ord_id;
    std::string exchange_order_id;
    std::string symbol;
    std::string side;
    double qty;
    double price;
    std::string state;
    uint64_t timestamp_us;
    std::string error_message;
};

struct BinancePosition {
    std::string symbol;
    double qty;
    double avg_price;
    double unrealized_pnl;
    double margin_used;
    uint64_t timestamp_us;
};

struct BinanceTrade {
    std::string cl_ord_id;
    std::string exchange_order_id;
    std::string symbol;
    std::string side;
    double qty;
    double price;
    double commission;
    uint64_t timestamp_us;
};

struct BinanceBalance {
    std::string asset;
    double free_balance;
    double locked_balance;
    double total_balance;
    uint64_t timestamp_us;
};

// Binance data fetcher interface
class IBinanceDataFetcher {
public:
    virtual ~IBinanceDataFetcher() = default;
    
    // Fetch current state from Binance
    virtual std::vector<BinanceOrder> get_active_orders() = 0;
    virtual std::vector<BinanceOrder> get_order_history(const std::string& symbol = "", 
                                                      uint64_t start_time = 0, 
                                                      uint64_t end_time = 0) = 0;
    virtual std::vector<BinancePosition> get_positions() = 0;
    virtual std::vector<BinanceTrade> get_trade_history(const std::string& symbol = "",
                                                      uint64_t start_time = 0,
                                                      uint64_t end_time = 0) = 0;
    virtual std::vector<BinanceBalance> get_balances() = 0;
    
    // Health check
    virtual bool is_connected() const = 0;
    virtual std::string get_exchange_name() const = 0;
};

// Binance data fetcher implementation
class BinanceDataFetcher : public IBinanceDataFetcher {
public:
    BinanceDataFetcher();
    ~BinanceDataFetcher() = default;
    
    // IBinanceDataFetcher interface
    std::vector<BinanceOrder> get_active_orders() override;
    std::vector<BinanceOrder> get_order_history(const std::string& symbol = "", 
                                               uint64_t start_time = 0, 
                                               uint64_t end_time = 0) override;
    std::vector<BinancePosition> get_positions() override;
    std::vector<BinanceTrade> get_trade_history(const std::string& symbol = "",
                                               uint64_t start_time = 0,
                                               uint64_t end_time = 0) override;
    std::vector<BinanceBalance> get_balances() override;
    
    bool is_connected() const override;
    std::string get_exchange_name() const override { return "BINANCE"; }
    
    // Configuration
    void set_api_credentials(const std::string& api_key, const std::string& api_secret);
    void set_base_url(const std::string& url);
    void set_testnet_mode(bool enabled);
    
    // HTTP request helper
    std::string make_request(const std::string& endpoint, const std::string& method = "GET", 
                            const std::string& body = "", bool is_signed = false);

private:
    std::string generate_signature(const std::string& data);
    std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body);
    
    std::string api_key_;
    std::string api_secret_;
    std::string base_url_;
    bool testnet_mode_{false};
    bool connected_{false};
    
    mutable std::mutex mutex_;
};

} // namespace binance
