#pragma once
#include "../exchanges/i_exchange_oms.hpp"
#include "../exchanges/i_exchange_pms.hpp"
#include "../exchanges/i_exchange_data_fetcher.hpp"
#include "../exchanges/i_exchange_subscriber.hpp"
#include "../proto/order.pb.h"
#include "../proto/position.pb.h"
#include "../proto/market_data.pb.h"
#include <memory>
#include <vector>
#include <map>

namespace framework_tests {

/**
 * Mock implementation of IExchangeOMS for testing framework components
 */
class MockExchangeOMS : public IExchangeOMS {
private:
    bool connected_{false};
    bool authenticated_{false};
    std::string api_key_;
    std::string secret_;
    OrderStatusCallback order_callback_;
    std::vector<proto::OrderEvent> order_events_;

public:
    MockExchangeOMS() = default;
    virtual ~MockExchangeOMS() = default;

    // Connection management
    bool connect() override {
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool is_connected() const override {
        return connected_;
    }

    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override {
        api_key_ = api_key;
        secret_ = secret;
        authenticated_ = true;
    }

    bool is_authenticated() const override {
        return authenticated_;
    }

    // Order management
    bool cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) override {
        if (!connected_ || !authenticated_) return false;
        
        proto::OrderEvent event;
        event.set_cl_ord_id(cl_ord_id);
        event.set_exch_ord_id(exch_ord_id);
        event.set_status("CANCELLED");
        
        order_events_.push_back(event);
        if (order_callback_) {
            order_callback_(event);
        }
        return true;
    }

    bool replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) override {
        if (!connected_ || !authenticated_) return false;
        
        proto::OrderEvent event;
        event.set_cl_ord_id(cl_ord_id);
        event.set_status("REPLACED");
        
        order_events_.push_back(event);
        if (order_callback_) {
            order_callback_(event);
        }
        return true;
    }

    proto::OrderEvent get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) override {
        proto::OrderEvent event;
        event.set_cl_ord_id(cl_ord_id);
        event.set_exch_ord_id(exch_ord_id);
        event.set_status("FILLED");
        return event;
    }

    bool place_market_order(const std::string& symbol, const std::string& side, double quantity) override {
        if (!connected_ || !authenticated_) return false;
        
        proto::OrderEvent event;
        event.set_symbol(symbol);
        event.set_side(side == "BUY" ? proto::Side::BUY : proto::Side::SELL);
        event.set_qty(quantity);
        event.set_status("FILLED");
        
        order_events_.push_back(event);
        if (order_callback_) {
            order_callback_(event);
        }
        return true;
    }

    bool place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) override {
        if (!connected_ || !authenticated_) return false;
        
        proto::OrderEvent event;
        event.set_symbol(symbol);
        event.set_side(side == "BUY" ? proto::Side::BUY : proto::Side::SELL);
        event.set_qty(quantity);
        event.set_price(price);
        event.set_status("NEW");
        
        order_events_.push_back(event);
        if (order_callback_) {
            order_callback_(event);
        }
        return true;
    }

    void set_order_status_callback(OrderStatusCallback callback) override {
        order_callback_ = callback;
    }

    // Test helper methods
    const std::vector<proto::OrderEvent>& get_order_events() const {
        return order_events_;
    }

    void clear_order_events() {
        order_events_.clear();
    }

    void simulate_order_event(const proto::OrderEvent& event) {
        if (order_callback_) {
            order_callback_(event);
        }
    }
};

/**
 * Mock implementation of IExchangePMS for testing framework components
 */
class MockExchangePMS : public IExchangePMS {
private:
    bool connected_{false};
    bool authenticated_{false};
    std::string api_key_;
    std::string secret_;
    PositionUpdateCallback position_callback_;
    std::vector<proto::PositionUpdate> position_updates_;

public:
    MockExchangePMS() = default;
    virtual ~MockExchangePMS() = default;

    // Connection management
    bool connect() override {
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool is_connected() const override {
        return connected_;
    }

    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override {
        api_key_ = api_key;
        secret_ = secret;
        authenticated_ = true;
    }

    bool is_authenticated() const override {
        return authenticated_;
    }

    void set_position_update_callback(PositionUpdateCallback callback) override {
        position_callback_ = callback;
    }

    // Test helper methods
    void simulate_position_update(const proto::PositionUpdate& update) {
        position_updates_.push_back(update);
        if (position_callback_) {
            position_callback_(update);
        }
    }

    const std::vector<proto::PositionUpdate>& get_position_updates() const {
        return position_updates_;
    }

    void clear_position_updates() {
        position_updates_.clear();
    }
};

/**
 * Mock implementation of IExchangeDataFetcher for testing framework components
 */
class MockExchangeDataFetcher : public IExchangeDataFetcher {
private:
    bool authenticated_{false};
    std::string api_key_;
    std::string secret_;
    std::vector<proto::OrderRequest> open_orders_;
    std::vector<proto::PositionUpdate> positions_;
    std::map<std::string, std::string> account_info_;

public:
    MockExchangeDataFetcher() = default;
    virtual ~MockExchangeDataFetcher() = default;

    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override {
        api_key_ = api_key;
        secret_ = secret;
        authenticated_ = true;
    }

    bool is_authenticated() const override {
        return authenticated_;
    }

    // Data fetching
    std::vector<proto::OrderRequest> get_open_orders() override {
        return open_orders_;
    }

    std::vector<proto::PositionUpdate> get_positions() override {
        return positions_;
    }

    std::map<std::string, std::string> get_account_info() override {
        return account_info_;
    }

    std::vector<proto::OrderRequest> get_order_history(const std::string& symbol, int limit) override {
        std::vector<proto::OrderRequest> history;
        // Return subset of open_orders_ as history
        for (const auto& order : open_orders_) {
            if (symbol.empty() || order.symbol() == symbol) {
                history.push_back(order);
                if (limit > 0 && history.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }
        return history;
    }

    // Test helper methods
    void set_open_orders(const std::vector<proto::OrderRequest>& orders) {
        open_orders_ = orders;
    }

    void set_positions(const std::vector<proto::PositionUpdate>& positions) {
        positions_ = positions;
    }

    void set_account_info(const std::map<std::string, std::string>& info) {
        account_info_ = info;
    }

    void add_open_order(const proto::OrderRequest& order) {
        open_orders_.push_back(order);
    }

    void add_position(const proto::PositionUpdate& position) {
        positions_.push_back(position);
    }
};

/**
 * Mock implementation of IExchangeSubscriber for testing framework components
 */
class MockExchangeSubscriber : public IExchangeSubscriber {
private:
    bool connected_{false};
    OrderbookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    std::vector<std::string> subscribed_symbols_;
    std::vector<proto::OrderBookSnapshot> orderbook_snapshots_;
    std::vector<proto::Trade> trades_;

public:
    MockExchangeSubscriber() = default;
    virtual ~MockExchangeSubscriber() = default;

    // Connection management
    bool connect() override {
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool is_connected() const override {
        return connected_;
    }

    // Subscription management
    bool subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) override {
        if (!connected_) return false;
        subscribed_symbols_.push_back(symbol);
        return true;
    }

    bool subscribe_trades(const std::string& symbol) override {
        if (!connected_) return false;
        subscribed_symbols_.push_back(symbol);
        return true;
    }

    bool unsubscribe(const std::string& symbol) override {
        auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
        if (it != subscribed_symbols_.end()) {
            subscribed_symbols_.erase(it);
            return true;
        }
        return false;
    }

    void set_orderbook_callback(OrderbookCallback callback) override {
        orderbook_callback_ = callback;
    }

    void set_trade_callback(TradeCallback callback) override {
        trade_callback_ = callback;
    }

    // Test helper methods
    void simulate_orderbook_update(const proto::OrderBookSnapshot& snapshot) {
        orderbook_snapshots_.push_back(snapshot);
        if (orderbook_callback_) {
            orderbook_callback_(snapshot);
        }
    }

    void simulate_trade_update(const proto::Trade& trade) {
        trades_.push_back(trade);
        if (trade_callback_) {
            trade_callback_(trade);
        }
    }

    const std::vector<std::string>& get_subscribed_symbols() const {
        return subscribed_symbols_;
    }

    const std::vector<proto::OrderBookSnapshot>& get_orderbook_snapshots() const {
        return orderbook_snapshots_;
    }

    const std::vector<proto::Trade>& get_trades() const {
        return trades_;
    }

    void clear_data() {
        orderbook_snapshots_.clear();
        trades_.clear();
    }
};

} // namespace framework_tests
