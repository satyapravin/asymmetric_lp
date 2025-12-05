# Strategy Development Guide

This guide explains how to implement a new trading strategy in the C++ trading system.

## Overview

The strategy framework is built around the `AbstractStrategy` base class, which provides a clean interface for strategy implementation while handling all the complex order management, state tracking, and ZMQ communication through the `StrategyContainer` and `MiniOMS`.

## Architecture

### Strategy Framework Components

```
┌─────────────────────────────────────────────────────────────────┐
│                        Trader Process                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                StrategyContainer                        │   │
│  │  - Holds single AbstractStrategy instance              │   │
│  │  - Manages ZMQ adapters (OMS, MDS, PMS)                │   │
│  │  - Delegates events to strategy                        │   │
│  │  - Routes orders through MiniOMS                       │   │
│  └─────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    MiniOMS                             │   │
│  │  - Order state management                              │   │
│  │  - State machine transitions                           │   │
│  │  - ZMQ adapter routing                                │   │
│  │  - Order statistics and queries                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                AbstractStrategy                         │   │
│  │  - Pure virtual interface                              │   │
│  │  - Event handlers                                      │   │
│  │  - Strategy logic implementation                       │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **Strategy Focuses on Logic**: Strategy only implements trading logic, not order management
2. **Container Handles Communication**: StrategyContainer manages all ZMQ communication
3. **MiniOMS Manages State**: All order state tracking is handled by MiniOMS
4. **Clean Separation**: Strategy doesn't know about ZMQ adapters or exchange details

## AbstractStrategy Interface

### Required Methods

```cpp
class AbstractStrategy {
public:
    // Constructor
    AbstractStrategy(const std::string& name);
    
    // Lifecycle management
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const;
    
    // Event handlers (implement these)
    virtual void on_market_data(const proto::OrderBookSnapshot& orderbook) = 0;
    virtual void on_order_event(const proto::OrderEvent& order_event) = 0;
    virtual void on_position_update(const proto::PositionUpdate& position) = 0;
    virtual void on_trade_execution(const proto::Trade& trade) = 0;
    
    // Configuration
    virtual void set_symbol(const std::string& symbol);
    virtual void set_exchange(const std::string& exchange);
    
    // Risk management (optional overrides)
    virtual void set_max_position_size(double max_size);
    virtual void set_max_order_size(double max_size);
    virtual void set_max_daily_loss(double max_loss);
    
    // Getters
    const std::string& get_name() const;
    const std::string& get_symbol() const;
    const std::string& get_exchange() const;
    bool is_enabled() const;
};
```

## Implementation Steps

### Step 1: Create Strategy Directory Structure

```
cpp/strategies/your_strategy/
├── your_strategy.hpp
├── your_strategy.cpp
├── models/
│   ├── your_model.hpp
│   └── your_model.cpp
└── README.md
```

### Step 2: Implement Strategy Header

```cpp
// your_strategy.hpp
#pragma once
#include "../base_strategy/abstract_strategy.hpp"
#include "models/your_model.hpp"
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

class YourStrategy : public AbstractStrategy {
public:
    // Constructor
    YourStrategy(const std::string& symbol, 
                 std::shared_ptr<YourModel> model);
    ~YourStrategy() override = default;
    
    // AbstractStrategy interface implementation
    void start() override;
    void stop() override;
    
    // Event handlers
    void on_market_data(const proto::OrderBookSnapshot& orderbook) override;
    void on_order_event(const proto::OrderEvent& order_event) override;
    void on_position_update(const proto::PositionUpdate& position) override;
    void on_trade_execution(const proto::Trade& trade) override;
    
    // Strategy-specific configuration
    void set_parameter1(double value);
    void set_parameter2(int value);
    
    // Strategy-specific queries
    double get_current_pnl() const;
    int get_active_orders_count() const;
    
private:
    // Strategy state
    std::string symbol_;
    std::shared_ptr<YourModel> model_;
    std::atomic<bool> running_;
    
    // Configuration
    std::atomic<double> parameter1_;
    std::atomic<int> parameter2_;
    
    // Internal state
    double current_position_;
    double current_pnl_;
    std::unordered_map<std::string, double> active_orders_;
    mutable std::mutex state_mutex_;
    
    // Internal methods
    void process_market_data(const proto::OrderBookSnapshot& orderbook);
    void update_quotes();
    void manage_risk();
    std::string generate_order_id() const;
    void log_strategy_event(const std::string& event);
};
```

### Step 3: Implement Strategy Logic

```cpp
// your_strategy.cpp
#include "your_strategy.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

YourStrategy::YourStrategy(const std::string& symbol, 
                          std::shared_ptr<YourModel> model)
    : AbstractStrategy("YourStrategy"), 
      symbol_(symbol), 
      model_(model),
      running_(false),
      parameter1_(1.0),
      parameter2_(100),
      current_position_(0.0),
      current_pnl_(0.0) {
}

void YourStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    std::cout << "[YOUR_STRATEGY] Starting strategy for " << symbol_ << std::endl;
    running_.store(true);
    
    // Initialize strategy state
    current_position_ = 0.0;
    current_pnl_ = 0.0;
    
    // Start strategy-specific logic
    log_strategy_event("Strategy started");
}

void YourStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[YOUR_STRATEGY] Stopping strategy" << std::endl;
    running_.store(false);
    
    // Clean up strategy state
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        active_orders_.clear();
    }
    
    log_strategy_event("Strategy stopped");
}

void YourStrategy::on_market_data(const proto::OrderBookSnapshot& orderbook) {
    if (!running_.load() || orderbook.symbol() != symbol_) {
        return;
    }
    
    // Process market data
    process_market_data(orderbook);
    
    // Update strategy logic
    update_quotes();
}

void YourStrategy::on_order_event(const proto::OrderEvent& order_event) {
    if (!running_.load() || order_event.symbol() != symbol_) {
        return;
    }
    
    std::cout << "[YOUR_STRATEGY] Order event: " << order_event.cl_ord_id() 
              << " - " << order_event.event_type() << std::endl;
    
    // Update strategy state based on order events
    if (order_event.event_type() == proto::FILL) {
        // Handle fill
        double fill_qty = order_event.fill_qty();
        double fill_price = order_event.fill_price();
        
        // Update position (strategy tracks its own position for logic)
        if (order_event.side() == proto::BUY) {
            current_position_ += fill_qty;
        } else {
            current_position_ -= fill_qty;
        }
        
        // Update PnL
        current_pnl_ += fill_qty * fill_price;
        
        log_strategy_event("Order filled: " + order_event.cl_ord_id() + 
                          " qty=" + std::to_string(fill_qty) + 
                          " price=" + std::to_string(fill_price));
    }
}

void YourStrategy::on_position_update(const proto::PositionUpdate& position) {
    if (!running_.load() || position.symbol() != symbol_) {
        return;
    }
    
    // Update strategy's understanding of position
    // Note: This is the "official" position from the exchange
    double exchange_position = position.qty();
    
    std::cout << "[YOUR_STRATEGY] Position update: " << symbol_ 
              << " qty=" << exchange_position << std::endl;
    
    // Reconcile with strategy's internal position tracking
    if (std::abs(current_position_ - exchange_position) > 0.001) {
        std::cout << "[YOUR_STRATEGY] Position mismatch detected!" << std::endl;
        // Handle position reconciliation
    }
}

void YourStrategy::on_trade_execution(const proto::Trade& trade) {
    if (!running_.load() || trade.symbol() != symbol_) {
        return;
    }
    
    std::cout << "[YOUR_STRATEGY] Trade execution: " << trade.symbol() 
              << " " << trade.qty() << " @ " << trade.price() << std::endl;
    
    // Update strategy statistics
    log_strategy_event("Trade executed: " + std::to_string(trade.qty()) + 
                      " @ " + std::to_string(trade.price()));
}

void YourStrategy::process_market_data(const proto::OrderBookSnapshot& orderbook) {
    // Extract market data
    double best_bid = 0.0, best_ask = 0.0;
    
    if (orderbook.bids_size() > 0) {
        best_bid = orderbook.bids(0).price();
    }
    if (orderbook.asks_size() > 0) {
        best_ask = orderbook.asks(0).price();
    }
    
    if (best_bid > 0.0 && best_ask > 0.0) {
        double spread = best_ask - best_bid;
        double mid_price = (best_bid + best_ask) / 2.0;
        
        // Update model with new market data
        if (model_) {
            model_->update_market_data(best_bid, best_ask, mid_price, spread);
        }
        
        // Log market data periodically
        static int tick_count = 0;
        if (++tick_count % 100 == 0) {
            std::cout << "[YOUR_STRATEGY] Market: " << symbol_ 
                      << " bid=" << best_bid << " ask=" << best_ask 
                      << " spread=" << spread << std::endl;
        }
    }
}

void YourStrategy::update_quotes() {
    if (!model_) {
        return;
    }
    
    // Get model recommendations
    auto recommendations = model_->get_recommendations();
    
    // Process recommendations and place orders
    for (const auto& rec : recommendations) {
        if (rec.action == "BUY" || rec.action == "SELL") {
            // Generate order ID
            std::string order_id = generate_order_id();
            
            // Determine order parameters
            proto::Side side = (rec.action == "BUY") ? proto::BUY : proto::SELL;
            double quantity = rec.quantity;
            double price = rec.price;
            
            // Track order in strategy state
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                active_orders_[order_id] = quantity;
            }
            
            // Note: Strategy doesn't actually send orders directly
            // The StrategyContainer will handle order sending through MiniOMS
            // This is just for strategy logic and tracking
            
            log_strategy_event("Order recommendation: " + order_id + 
                              " " + rec.action + " " + std::to_string(quantity) + 
                              " @ " + std::to_string(price));
        }
    }
}

void YourStrategy::manage_risk() {
    // Implement risk management logic
    double max_position = get_max_position_size();
    
    if (std::abs(current_position_) > max_position) {
        std::cout << "[YOUR_STRATEGY] Risk limit exceeded: position=" 
                  << current_position_ << " max=" << max_position << std::endl;
        
        // Implement risk mitigation
        log_strategy_event("Risk limit exceeded - implementing mitigation");
    }
}

std::string YourStrategy::generate_order_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    std::stringstream ss;
    ss << "YS_" << symbol_ << "_" << dis(gen) << "_" 
       << std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();
    return ss.str();
}

void YourStrategy::log_strategy_event(const std::string& event) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "[YOUR_STRATEGY] " << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << " " << event << std::endl;
}

// Configuration methods
void YourStrategy::set_parameter1(double value) {
    parameter1_.store(value);
    std::cout << "[YOUR_STRATEGY] Parameter1 set to " << value << std::endl;
}

void YourStrategy::set_parameter2(int value) {
    parameter2_.store(value);
    std::cout << "[YOUR_STRATEGY] Parameter2 set to " << value << std::endl;
}

// Query methods
double YourStrategy::get_current_pnl() const {
    return current_pnl_;
}

int YourStrategy::get_active_orders_count() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return active_orders_.size();
}
```

### Step 4: Create Strategy Model (Optional)

```cpp
// models/your_model.hpp
#pragma once
#include <vector>
#include <memory>

struct Recommendation {
    std::string action;  // "BUY", "SELL", "HOLD"
    double quantity;
    double price;
    double confidence;
};

class YourModel {
public:
    YourModel();
    ~YourModel() = default;
    
    void update_market_data(double bid, double ask, double mid, double spread);
    std::vector<Recommendation> get_recommendations();
    
private:
    double last_bid_;
    double last_ask_;
    double last_mid_;
    double last_spread_;
    
    // Model-specific logic
    void calculate_signals();
    bool should_buy() const;
    bool should_sell() const;
    double calculate_quantity() const;
    double calculate_price() const;
};
```

### Step 5: Update Build System

Add to `cpp/strategies/CMakeLists.txt`:

```cmake
# Your Strategy
add_library(your_strategy STATIC
    your_strategy/your_strategy.cpp
    your_strategy/models/your_model.cpp
)

target_link_libraries(your_strategy PRIVATE base_strategy utils)
```

### Step 6: Create Strategy Configuration

```ini
# config/your_strategy.ini
[GLOBAL]
STRATEGY_NAME=YourStrategy
SYMBOL=BTCUSDT
EXCHANGE=BINANCE
LOG_LEVEL=INFO

[STRATEGY_PARAMETERS]
PARAMETER1=1.5
PARAMETER2=200
MAX_POSITION_SIZE=10.0
MAX_ORDER_SIZE=1.0

[RISK_MANAGEMENT]
MAX_DAILY_LOSS=1000.0
POSITION_LIMIT_CHECK_INTERVAL_MS=1000
```

## Integration with StrategyContainer

### How Orders Are Actually Sent

The strategy doesn't send orders directly. Instead, the `StrategyContainer` handles order management:

```cpp
// In StrategyContainer
bool StrategyContainer::send_order(const std::string& cl_ord_id,
                                  const std::string& symbol,
                                  proto::Side side,
                                  proto::OrderType type,
                                  double qty,
                                  double price) {
    if (!strategy_ || !strategy_->is_running()) {
        return false;
    }
    
    // Strategy validates the order (if needed)
    // Then MiniOMS handles the actual sending
    return mini_oms_->send_order(cl_ord_id, symbol, side, type, qty, price);
}
```

### Accessing Order State

Strategies can query order state through the MiniOMS:

```cpp
// In your strategy
void YourStrategy::check_order_status(const std::string& order_id) {
    // Access MiniOMS through StrategyContainer
    // This would need to be exposed through the container interface
    auto order_state = mini_oms_->get_order_state(order_id);
    
    if (order_state.state == OrderState::FILLED) {
        // Handle filled order
    }
}
```

## Testing Your Strategy

### Unit Tests

```cpp
// tests/unit/strategies/test_your_strategy.cpp
#include "doctest.h"
#include "../../strategies/your_strategy/your_strategy.hpp"
#include "../../strategies/your_strategy/models/your_model.hpp"

TEST_SUITE("YourStrategy") {
    TEST_CASE("Constructor and Basic Properties") {
        auto model = std::make_shared<YourModel>();
        YourStrategy strategy("BTCUSDT", model);
        
        CHECK(strategy.get_name() == "YourStrategy");
        CHECK(strategy.get_symbol() == "BTCUSDT");
        CHECK_FALSE(strategy.is_running());
    }
    
    TEST_CASE("Start and Stop") {
        auto model = std::make_shared<YourModel>();
        YourStrategy strategy("BTCUSDT", model);
        
        CHECK_FALSE(strategy.is_running());
        strategy.start();
        CHECK(strategy.is_running());
        strategy.stop();
        CHECK_FALSE(strategy.is_running());
    }
    
    TEST_CASE("Market Data Processing") {
        auto model = std::make_shared<YourModel>();
        YourStrategy strategy("BTCUSDT", model);
        strategy.start();
        
        proto::OrderBookSnapshot orderbook;
        orderbook.set_symbol("BTCUSDT");
        
        // Add bid/ask levels
        auto* bid = orderbook.add_bids();
        bid->set_price(50000.0);
        bid->set_quantity(1.0);
        
        auto* ask = orderbook.add_asks();
        ask->set_price(50010.0);
        ask->set_quantity(1.0);
        
        // Process market data
        strategy.on_market_data(orderbook);
        
        // Verify strategy processed the data
        CHECK(strategy.is_running());
        
        strategy.stop();
    }
    
    TEST_CASE("Order Event Handling") {
        auto model = std::make_shared<YourModel>();
        YourStrategy strategy("BTCUSDT", model);
        strategy.start();
        
        proto::OrderEvent order_event;
        order_event.set_cl_ord_id("TEST_ORDER_1");
        order_event.set_symbol("BTCUSDT");
        order_event.set_event_type(proto::FILL);
        order_event.set_fill_qty(1.0);
        order_event.set_fill_price(50000.0);
        
        strategy.on_order_event(order_event);
        
        // Verify strategy handled the event
        CHECK(strategy.get_current_pnl() > 0.0);
        
        strategy.stop();
    }
}
```

### Integration Tests

```cpp
TEST_CASE("Strategy Integration with Container") {
    // Test strategy integration with StrategyContainer
    auto model = std::make_shared<YourModel>();
    auto strategy = std::make_shared<YourStrategy>("BTCUSDT", model);
    
    StrategyContainer container;
    container.set_strategy(strategy);
    
    // Test container integration
    container.start();
    CHECK(container.is_running());
    
    // Test order management through container
    bool order_sent = container.send_order("TEST_ORDER", "BTCUSDT", 
                                          proto::BUY, proto::LIMIT, 1.0, 50000.0);
    CHECK(order_sent);
    
    container.stop();
}
```

## Best Practices

### Strategy Design

1. **Keep Strategy Logic Pure**: Don't mix order management with strategy logic
2. **Use Atomic Operations**: For thread-safe configuration parameters
3. **Implement Proper State Management**: Track strategy state independently
4. **Handle Edge Cases**: Consider market closed, connection drops, etc.

### Performance

1. **Minimize Allocations**: Use stack allocation where possible
2. **Efficient Data Structures**: Choose appropriate containers
3. **Avoid Blocking Operations**: Keep event handlers fast
4. **Profile Critical Paths**: Optimize hot code paths

### Error Handling

1. **Validate Inputs**: Check all incoming data
2. **Graceful Degradation**: Continue operating with reduced functionality
3. **Comprehensive Logging**: Log all important events
4. **Recovery Mechanisms**: Implement automatic recovery where possible

### Testing

1. **Unit Test All Logic**: Test strategy logic in isolation
2. **Mock Dependencies**: Use mocks for external dependencies
3. **Test Edge Cases**: Cover error conditions and boundary cases
4. **Performance Testing**: Benchmark critical operations

## Common Patterns

### Market Making Strategy

```cpp
void MarketMakingStrategy::update_quotes() {
    double mid_price = get_mid_price();
    double spread = get_spread();
    
    // Place buy order below market
    double buy_price = mid_price - spread * 0.5;
    place_limit_order("BUY", buy_price, quote_size_);
    
    // Place sell order above market
    double sell_price = mid_price + spread * 0.5;
    place_limit_order("SELL", sell_price, quote_size_);
}
```

### Trend Following Strategy

```cpp
void TrendFollowingStrategy::process_market_data(const proto::OrderBookSnapshot& orderbook) {
    double current_price = get_mid_price(orderbook);
    
    // Calculate moving average
    price_history_.push_back(current_price);
    if (price_history_.size() > lookback_period_) {
        price_history_.erase(price_history_.begin());
    }
    
    double ma = calculate_moving_average(price_history_);
    
    // Generate signals
    if (current_price > ma * (1.0 + threshold_)) {
        generate_buy_signal();
    } else if (current_price < ma * (1.0 - threshold_)) {
        generate_sell_signal();
    }
}
```

### Arbitrage Strategy

```cpp
void ArbitrageStrategy::check_arbitrage_opportunities() {
    double price_exchange1 = get_price_from_exchange1();
    double price_exchange2 = get_price_from_exchange2();
    
    double spread = std::abs(price_exchange1 - price_exchange2);
    double min_profit = spread - trading_costs_;
    
    if (min_profit > min_profit_threshold_) {
        if (price_exchange1 < price_exchange2) {
            // Buy on exchange1, sell on exchange2
            execute_arbitrage("EXCHANGE1", "BUY", "EXCHANGE2", "SELL");
        } else {
            // Buy on exchange2, sell on exchange1
            execute_arbitrage("EXCHANGE2", "BUY", "EXCHANGE1", "SELL");
        }
    }
}
```

---

**This guide provides the foundation for implementing trading strategies. Focus on clean, testable code and let the framework handle the complex order management and communication details.**
