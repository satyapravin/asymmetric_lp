#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <optional>
#include <thread>
#include <functional>
#include <map>
#include "../base_strategy/abstract_strategy.hpp"
#include "../../utils/oms/order_state.hpp"
#include "models/glft_target.hpp"

// Market Making Strategy that inherits from AbstractStrategy
class MarketMakingStrategy : public AbstractStrategy {
public:
  using OrderStateCallback = std::function<void(const OrderStateInfo& order_info)>;
  
  // Constructor with GLFT model
  MarketMakingStrategy(const std::string& symbol,
                      std::shared_ptr<GlftTarget> glft_model);
  
  ~MarketMakingStrategy() = default;
  
  // AbstractStrategy interface implementation
  void start() override;
  void stop() override;
  bool is_running() const override { return running_.load(); }
  
  
  void set_symbol(const std::string& symbol) override { symbol_ = symbol; }
  void set_exchange(const std::string& exchange) override { exchange_ = exchange; }
  
      // Event handlers
      void on_market_data(const proto::OrderBookSnapshot& orderbook) override;
      void on_order_event(const proto::OrderEvent& order_event) override;
      void on_position_update(const proto::PositionUpdate& position) override;
      void on_trade_execution(const proto::Trade& trade) override;
      void on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) override;
  
  // Order management (Strategy calls Container)
  // Note: Strategy doesn't implement order management - it calls the container
  // The container will be set by the trader process
  
  // Configuration
  void set_inventory_delta(double delta) { current_inventory_delta_.store(delta); }
  void set_min_spread_bps(double bps) { min_spread_bps_ = bps; }
  void set_max_position_size(double size) { max_position_size_ = size; }
  void set_quote_size(double size) { quote_size_ = size; }
  
  // Order state queries (delegated to Mini OMS)
  OrderStateInfo get_order_state(const std::string& cl_ord_id);
  std::vector<OrderStateInfo> get_active_orders();
  std::vector<OrderStateInfo> get_all_orders();
  
      // Position queries (delegated to Mini PMS via Container)
      std::optional<trader::PositionInfo> get_position(const std::string& exchange,
                                                      const std::string& symbol) const override;
      std::vector<trader::PositionInfo> get_all_positions() const override;
      std::vector<trader::PositionInfo> get_positions_by_exchange(const std::string& exchange) const override;
      std::vector<trader::PositionInfo> get_positions_by_symbol(const std::string& symbol) const override;

      // Account balance queries (delegated to Mini PMS via Container)
      std::optional<trader::AccountBalanceInfo> get_account_balance(const std::string& exchange,
                                                                   const std::string& instrument) const override;
      std::vector<trader::AccountBalanceInfo> get_all_account_balances() const override;
      std::vector<trader::AccountBalanceInfo> get_account_balances_by_exchange(const std::string& exchange) const override;
      std::vector<trader::AccountBalanceInfo> get_account_balances_by_instrument(const std::string& instrument) const override;
  
  // Statistics
  struct Statistics {
    std::atomic<uint64_t> total_orders{0};
    std::atomic<uint64_t> filled_orders{0};
    std::atomic<uint64_t> cancelled_orders{0};
    std::atomic<double> total_volume{0.0};
    std::atomic<double> total_pnl{0.0};
    
    void reset() {
      total_orders.store(0);
      filled_orders.store(0);
      cancelled_orders.store(0);
      total_volume.store(0.0);
      total_pnl.store(0.0);
    }
  };
  
  const Statistics& get_statistics() const { return statistics_; }
  
  // Callbacks
  void set_order_state_callback(OrderStateCallback callback) { order_state_callback_ = callback; }

private:
  // Core components
  std::string symbol_;
  std::string exchange_;
  std::shared_ptr<GlftTarget> glft_model_;
  std::atomic<bool> running_{false};
  
  // Configuration
  std::atomic<double> current_inventory_delta_{0.0};
  double min_spread_bps_{5.0};  // 5 basis points minimum spread
  double max_position_size_{100.0};
  double quote_size_{1.0};
  
  // Statistics
  Statistics statistics_;
  
  // Callbacks
  OrderStateCallback order_state_callback_;
  
  // Internal methods
  void process_orderbook(const proto::OrderBookSnapshot& orderbook);
  void update_quotes();
  void manage_inventory();
  std::string generate_order_id() const;
};