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
#include "market_making_strategy_config.hpp"

// Market Making Strategy that inherits from AbstractStrategy
class MarketMakingStrategy : public AbstractStrategy {
public:
  using OrderStateCallback = std::function<void(const OrderStateInfo& order_info)>;
  
  // Constructor with GLFT model
  MarketMakingStrategy(const std::string& symbol,
                      std::shared_ptr<GlftTarget> glft_model);
  
  // Constructor with config (creates GLFT model from config)
  MarketMakingStrategy(const std::string& symbol,
                      const MarketMakingStrategyConfig& config);
  
  // Load configuration from config manager
  void load_config(const config::ProcessConfigManager& config_manager,
                   const std::string& section = "market_making_strategy");
  
  // Load configuration from file
  bool load_config_from_file(const std::string& config_file,
                            const std::string& section = "market_making_strategy");
  
  // Apply configuration to strategy
  void apply_config(const MarketMakingStrategyConfig& config);
  
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
  void set_leverage(double leverage) { leverage_ = std::max(1.0, leverage); }  // Minimum leverage is 1.0x
  void set_base_quote_size_pct(double pct) { base_quote_size_pct_ = pct; }  // Percentage of balance
  void set_min_quote_size_pct(double pct) { min_quote_size_pct_ = pct; }  // Percentage of balance
  void set_max_quote_size_pct(double pct) { max_quote_size_pct_ = pct; }  // Percentage of balance
  
  // Legacy setters for backward compatibility (now set percentages)
  void set_quote_size(double size) { base_quote_size_pct_ = size; }  // Deprecated: use set_base_quote_size_pct
  void set_min_quote_size(double size) { min_quote_size_pct_ = size; }  // Deprecated: use set_min_quote_size_pct
  void set_max_quote_size(double size) { max_quote_size_pct_ = size; }  // Deprecated: use set_max_quote_size_pct
  
  // Quote update throttling configuration
  void set_min_price_change_bps(double bps) { min_price_change_bps_ = bps; }
  void set_min_inventory_change_pct(double pct) { min_inventory_change_pct_ = pct; }
  void set_quote_update_interval_ms(int ms) { quote_update_interval_ms_ = ms; }
  void set_min_quote_price_change_bps(double bps) { min_quote_price_change_bps_ = bps; }
  
  // DeFi position management (Uniswap V3 LP positions)
  struct DefiPosition {
    std::string pool_address;  // Uniswap V3 pool address
    std::string token0_address;
    std::string token1_address;
    double token0_amount;      // Amount of token0 in LP position
    double token1_amount;      // Amount of token1 in LP position
    double liquidity;          // Liquidity amount
    double range_lower;        // Lower price bound
    double range_upper;        // Upper price bound
    int fee_tier;              // Fee tier (e.g., 500 = 0.05%)
    
    DefiPosition() : token0_amount(0.0), token1_amount(0.0), liquidity(0.0),
                     range_lower(0.0), range_upper(0.0), fee_tier(0) {}
  };
  
  // Register/update DeFi position
  void update_defi_position(const DefiPosition& position);
  void remove_defi_position(const std::string& pool_address);
  std::vector<DefiPosition> get_defi_positions() const;
  
  // Combined inventory calculation
  struct CombinedInventory {
    double token0_total{0.0};  // Combined token0 (CeFi + DeFi)
    double token1_total{0.0};  // Combined token1 (CeFi + DeFi)
    double token0_cefi{0.0};   // CeFi token0 only
    double token1_cefi{0.0};   // CeFi token1 only
    double token0_defi{0.0};   // DeFi token0 only
    double token1_defi{0.0};   // DeFi token1 only
  };
  
  CombinedInventory calculate_combined_inventory(double spot_price) const;
  
  // Calculate CeFi-only inventory (for GLFT model)
  struct CeFiInventory {
    double token0{0.0};  // Collateral (USD)
    double token1{0.0}; // Position (contracts)
  };
  CeFiInventory calculate_cefi_inventory() const;
  
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
  double leverage_{1.0};              // Leverage multiplier (1.0 = no leverage, 2.0 = 2x, etc.)
  double base_quote_size_pct_{0.02};   // Base quote size as % of leveraged balance (2%)
  double min_quote_size_pct_{0.005};   // Minimum quote size as % of leveraged balance (0.5%)
  double max_quote_size_pct_{0.25};    // Maximum quote size as % of leveraged balance (25%)
  double micro_price_skew_alpha_{1.0};  // Alpha parameter to multiply micro price skew (default: 1.0 = full effect)
  double net_inventory_skew_gamma_{0.5};  // Gamma parameter to multiply net inventory skew (default: 0.5)
                                           // Net inventory = CeFi inventory + DeFi delta (both in contracts)
                                           // Normalized to % of collateral for skew calculation
  
  // DeFi position tracking
  mutable std::mutex defi_positions_mutex_;
  std::map<std::string, DefiPosition> defi_positions_;  // pool_address -> DefiPosition
  
  // Cached DeFi inventory flow (in contracts) - used to skew bid/ask quotes
  // Weighted combination of flow_5s, flow_1m, flow_5m
  // Positive = inventory increasing (skew quotes to encourage selling)
  // Negative = inventory decreasing (skew quotes to encourage buying)
  std::atomic<double> cached_defi_flow_contracts_{0.0};
  
  // Weights for inventory flow combination (default: use flow_1m primarily)
  double flow_5s_weight_{0.2};   // Weight for 5-second flow
  double flow_1m_weight_{0.6};   // Weight for 1-minute flow (primary)
  double flow_5m_weight_{0.2};   // Weight for 5-minute flow
  
  // Market data for GLFT calculations
  std::atomic<double> current_spot_price_{0.0};
  std::atomic<double> current_volatility_{0.02};  // Default 2% volatility
  
  // Best bid/ask from orderbook (for quote validation)
  mutable std::mutex orderbook_mutex_;
  double best_bid_{0.0};
  double best_ask_{0.0};
  
  // Cached orderbook for micro price calculation (top 5 levels)
  proto::OrderBookSnapshot cached_orderbook_;
  bool orderbook_cached_{false};
  
  // EWMA volatility calculation
  mutable std::mutex volatility_mutex_;
  double ewma_variance_{0.0};  // EWMA variance estimate
  double last_price_{0.0};     // Last price for return calculation
  bool volatility_initialized_{false};
  double ewma_decay_factor_{0.94};  // λ (lambda) - typically 0.94-0.97 for daily data
  
  // Quote update throttling (to avoid excessive order cancellations)
  mutable std::mutex quote_update_mutex_;
  std::chrono::system_clock::time_point last_quote_update_time_;
  double last_quote_bid_price_{0.0};
  double last_quote_ask_price_{0.0};
  double last_mid_price_{0.0};
  std::vector<std::string> active_order_ids_;  // Track active order IDs for cancellation
  
  // Quote update thresholds
  double min_price_change_bps_{5.0};      // Minimum price change (5 bps) to trigger update
  double min_inventory_change_pct_{1.0};  // Minimum inventory change (1%) to trigger update
  int quote_update_interval_ms_{5000};   // Minimum time between updates (5 seconds - reduces flickering)
  double min_quote_price_change_bps_{2.0}; // Minimum bid/ask price change (2 bps) to actually update quotes
  
  // Statistics
  Statistics statistics_;
  
  // Callbacks
  OrderStateCallback order_state_callback_;
  
  // Internal methods
  void process_orderbook(const proto::OrderBookSnapshot& orderbook);
  void update_quotes();
  void manage_inventory();
  std::string generate_order_id() const;
  
  // Micro price calculation (weighted mid price from top N levels)
  double calculate_micro_price(const proto::OrderBookSnapshot& orderbook, int num_levels = 5) const;
  double get_micro_price_skew() const;  // Returns (micro_price - mid_price) / mid_price
  double get_orderbook_imbalance() const;  // Returns (bid_qty - ask_qty) / (bid_qty + ask_qty), range [-1, 1]
  
  // Helper methods for inventory calculation
  double calculate_volatility_from_orderbook(const proto::OrderBookSnapshot& orderbook) const;
  void update_spot_price_from_orderbook(const proto::OrderBookSnapshot& orderbook);
  void update_ewma_volatility(double current_price);
  
  // Configuration for volatility
  void set_ewma_decay_factor(double lambda) { ewma_decay_factor_ = lambda; }
  double get_ewma_decay_factor() const { return ewma_decay_factor_; }
  
  // Quote update throttling logic
  bool should_update_quotes(double current_mid_price) const;
  
  // Configuration access
  MarketMakingStrategyConfig get_config() const;
};