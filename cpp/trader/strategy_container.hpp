#pragma once
#include <memory>
#include <string>
#include <atomic>
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"
#include "../proto/position.pb.h"
#include "../proto/acc_balance.pb.h"
#include "mini_oms.hpp"
#include "mini_pms.hpp"

// Forward declarations
class AbstractStrategy;
class ZmqOMSAdapter;
class ZmqMDSAdapter;
class ZmqPMSAdapter;

/**
 * Strategy Container Interface
 * 
 * Simple interface for trader process to contain and manage a single strategy.
 * The trader process instantiates one strategy and routes events to it.
 */
class IStrategyContainer {
public:
    virtual ~IStrategyContainer() = default;
    
    // Strategy lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    
        // Event handlers
        virtual void on_market_data(const proto::OrderBookSnapshot& orderbook) = 0;
        virtual void on_order_event(const proto::OrderEvent& order_event) = 0;
        virtual void on_position_update(const proto::PositionUpdate& position) = 0;
        virtual void on_trade_execution(const proto::Trade& trade) = 0;
        virtual void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) = 0;
    
    // Configuration
    virtual void set_symbol(const std::string& symbol) = 0;
    virtual void set_exchange(const std::string& exchange) = 0;
    virtual const std::string& get_name() const = 0;
    
    // ZMQ adapter setup
    virtual void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) = 0;
    virtual void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) = 0;
    virtual void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) = 0;
    
        // Position queries (Strategy can query positions via Container)
        virtual std::optional<trader::PositionInfo> get_position(const std::string& exchange,
                                                                const std::string& symbol) const = 0;
        virtual std::vector<trader::PositionInfo> get_all_positions() const = 0;
        virtual std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string& exchange) const = 0;
        virtual std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string& symbol) const = 0;
        
        // Account balance queries (Strategy can query balances via Container)
        virtual std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string& exchange,
                                                                             const std::string& instrument) const = 0;
        virtual std::vector<trader::AccountBalanceInfo> get_all_account_balances() const = 0;
        virtual std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const = 0;
        virtual std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const = 0;
};

// Forward declaration for concrete implementation
class StrategyContainer : public IStrategyContainer {
public:
    StrategyContainer();
    ~StrategyContainer();
    
    // Set the strategy instance
    void set_strategy(std::shared_ptr<AbstractStrategy> strategy);
    
    // Get the strategy instance (for testing)
    std::shared_ptr<AbstractStrategy> get_strategy() const { return strategy_; }
    
    // IStrategyContainer interface implementation
    void start() override;
    void stop() override;
    bool is_running() const override;
    
    // Event handlers
    void on_market_data(const proto::OrderBookSnapshot& orderbook) override;
    void on_order_event(const proto::OrderEvent& order_event) override;
    void on_position_update(const proto::PositionUpdate& position) override;
    void on_trade_execution(const proto::Trade& trade) override;
    void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) override;
    
    // Configuration
    void set_symbol(const std::string& symbol) override;
    void set_exchange(const std::string& exchange) override;
    const std::string& get_name() const override;
    
    // ZMQ adapter setup
    void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) override;
    void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) override;
    void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) override;
    
    // Position queries
    std::optional<trader::PositionInfo> get_position(const std::string& exchange, const std::string& symbol) const override;
    std::vector<trader::PositionInfo> get_all_positions() const override;
    std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string& exchange) const override;
    std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string& symbol) const override;
    
    // Account balance queries
    std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string& exchange, const std::string& instrument) const override;
    std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override;
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const override;
    std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const override;

private:
    std::shared_ptr<AbstractStrategy> strategy_;
    std::unique_ptr<MiniOMS> mini_oms_;
    std::unique_ptr<trader::MiniPMS> mini_pms_;
    std::shared_ptr<ZmqOMSAdapter> oms_adapter_;
    std::shared_ptr<ZmqMDSAdapter> mds_adapter_;
    std::shared_ptr<ZmqPMSAdapter> pms_adapter_;
    std::atomic<bool> running_{false};
    std::string symbol_;
    std::string exchange_;
    std::string name_;
};
