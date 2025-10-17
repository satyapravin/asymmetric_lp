#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <optional>
#include <thread>
#include <functional>
#include <map>
#include "../utils/mds/orderbook_binary.hpp"
#include "../utils/pms/position_binary.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/oms/enhanced_oms.hpp"
#include "../utils/oms/mock_exchange_oms.hpp"
#include "../utils/oms/order_state.hpp"
#include "models/glft_target.hpp"

// Unified market making strategy with integrated OMS, GLFT model, and inventory management
class MarketMakingStrategy {
public:
  using OrderBookCallback = std::function<void(const std::string& symbol,
                                             const std::vector<std::pair<double, double>>& bids,
                                             const std::vector<std::pair<double, double>>& asks,
                                             uint64_t timestamp_us)>;
  
  using OrderEventCallback = std::function<void(const OrderEvent& event)>;
  using OrderStateCallback = std::function<void(const OrderStateInfo& order_info)>;
  
  // Constructor with integrated OMS and GLFT model
  MarketMakingStrategy(const std::string& symbol,
                      std::shared_ptr<GlftTarget> glft_model,
                      const std::string& md_endpoint = "",
                      const std::string& md_topic = "",
                      const std::string& pos_endpoint = "",
                      const std::string& pos_topic = "",
                      const std::string& inventory_endpoint = "",
                      const std::string& inventory_topic = "");
  
  ~MarketMakingStrategy();
  
  void start();
  void stop();
  
  // Configuration
  void set_inventory_delta(double delta) { current_inventory_delta_.store(delta); }
  void set_min_spread_bps(double bps) { min_spread_bps_ = bps; }
  void set_max_position_size(double size) { max_position_size_ = size; }
  void set_quote_size(double size) { quote_size_ = size; }
  
  // Market data callback
  void on_orderbook_update(const std::string& symbol,
                          const std::vector<std::pair<double, double>>& bids,
                          const std::vector<std::pair<double, double>>& asks,
                          uint64_t timestamp_us);
  
  // Position callback
  void on_position_update(const std::string& symbol,
                         const std::string& exch,
                         double qty,
                         double avg_price);
  
  // Inventory callback (from DeFi)
  void on_inventory_update(const std::string& symbol,
                          double inventory_delta);
  
  // Order management
  void submit_order(const Order& order);
  void cancel_order(const std::string& cl_ord_id);
  void modify_order(const std::string& cl_ord_id, double new_price, double new_qty);
  
  // Exchange management
  void register_exchange(const std::string& exchange_name, std::shared_ptr<IExchangeOMS> oms);
  void disconnect_from_exchanges();
  
  // Order state queries
  OrderStateInfo get_order_state(const std::string& cl_ord_id);
  std::vector<OrderStateInfo> get_active_orders();
  std::vector<OrderStateInfo> get_all_orders();
  
  // Statistics
  struct OrderStats {
    size_t total_orders = 0;
    size_t filled_orders = 0;
    size_t cancelled_orders = 0;
    size_t rejected_orders = 0;
    double total_volume = 0.0;
    double filled_volume = 0.0;
  };
  OrderStats get_order_statistics();
  
  // Callbacks
  void set_order_event_callback(OrderEventCallback callback) { order_event_callback_ = callback; }
  void set_order_state_callback(OrderStateCallback callback) { order_state_callback_ = callback; }
  
  // Generic message handler for ZeroMQ feeds
  void on_message(const std::string& handler_name, const std::string& data);
  
private:
  void process_market_data();
  void process_position_data();
  void process_inventory_data();
  void process_order_events();
  
  // GLFT-based pricing
  std::pair<double, double> calculate_optimal_quotes(double mid_price, double inventory_delta);
  
  // Order management
  void update_quotes();
  void cancel_existing_quotes();
  
  std::string symbol_;
  std::shared_ptr<GlftTarget> glft_model_;
  
  // ZeroMQ subscribers
  std::unique_ptr<ZmqSubscriber> md_subscriber_;
  std::unique_ptr<ZmqSubscriber> pos_subscriber_;
  std::unique_ptr<ZmqSubscriber> inventory_subscriber_;
  
  // OMS integration
  std::unique_ptr<OMS> oms_;
  std::map<std::string, std::shared_ptr<IExchangeOMS>> exchange_oms_;
  
  // Threading
  std::atomic<bool> running_{false};
  std::thread md_thread_;
  std::thread pos_thread_;
  std::thread inventory_thread_;
  std::thread order_thread_;
  
  // State
  std::atomic<double> current_inventory_delta_{0.0};
  std::map<std::string, double> current_positions_; // symbol -> qty
  std::map<std::string, double> avg_prices_;       // symbol -> avg_price
  
  // Configuration
  double min_spread_bps_{10.0};  // 10 basis points minimum spread
  double max_position_size_{1.0}; // Maximum position size
  double quote_size_{0.1};       // Default quote size
  
  // Active orders
  std::string last_bid_order_id_;
  std::string last_ask_order_id_;
  
  // Callbacks
  OrderEventCallback order_event_callback_;
  OrderStateCallback order_state_callback_;
  
  // Feed enable flags (defaulted to false as handlers drive feeds)
  bool enable_market_data_{false};
  bool enable_positions_{false};
  bool enable_inventory_{false};
};