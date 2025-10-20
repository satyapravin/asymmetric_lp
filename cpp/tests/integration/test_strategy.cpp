#include "test_strategy.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace testing {

TestStrategy::TestStrategy() 
    : AbstractStrategy("TestStrategy"), running_(false) {
    test_results_.reset();
}

void TestStrategy::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    log_test_event("START", "Test strategy started");
    
    // Run configured test scenarios
    for (const auto& scenario : test_scenarios_) {
        if (scenario == "basic_order") {
            run_basic_order_test();
        } else if (scenario == "partial_fill") {
            run_partial_fill_test();
        } else if (scenario == "cancellation") {
            run_cancellation_test();
        } else if (scenario == "rejection") {
            run_rejection_test();
        } else if (scenario == "market_data") {
            run_market_data_test();
        }
        
        // Small delay between scenarios
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void TestStrategy::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    log_test_event("STOP", "Test strategy stopped");
}

void TestStrategy::on_market_data(const proto::OrderBookSnapshot& orderbook) {
    test_results_.market_data_received.fetch_add(1);
    
    log_test_event("MARKET_DATA", 
                   "Symbol: " + orderbook.symbol() + 
                   ", Bids: " + std::to_string(orderbook.bids_size()) +
                   ", Asks: " + std::to_string(orderbook.asks_size()));
}

void TestStrategy::on_order_event(const proto::OrderEvent& order_event) {
    std::string cl_ord_id = order_event.cl_ord_id();
    proto::OrderStatus status = order_event.status();
    
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        test_results_.order_statuses[cl_ord_id] = status;
        test_results_.order_timestamps[cl_ord_id] = std::chrono::system_clock::now();
    }
    
    switch (status) {
        case proto::OrderStatus::NEW:
            test_results_.orders_acked.fetch_add(1);
            log_test_event("ORDER_ACK", "Order " + cl_ord_id + " acknowledged");
            break;
            
        case proto::OrderStatus::REJECTED:
            test_results_.orders_rejected.fetch_add(1);
            log_test_event("ORDER_REJECT", "Order " + cl_ord_id + " rejected: " + order_event.reject_reason());
            break;
            
        case proto::OrderStatus::CANCELLED:
            test_results_.orders_cancelled.fetch_add(1);
            log_test_event("ORDER_CANCEL", "Order " + cl_ord_id + " cancelled");
            break;
            
        case proto::OrderStatus::PARTIALLY_FILLED:
            test_results_.orders_partial_filled.fetch_add(1);
            log_test_event("ORDER_PARTIAL_FILL", 
                         "Order " + cl_ord_id + " partially filled: " + 
                         std::to_string(order_event.filled_qty()) + "/" + std::to_string(order_event.qty()));
            break;
            
        case proto::OrderStatus::FILLED:
            test_results_.orders_filled.fetch_add(1);
            log_test_event("ORDER_FILLED", 
                         "Order " + cl_ord_id + " completely filled: " + std::to_string(order_event.filled_qty()));
            break;
            
        default:
            log_test_event("ORDER_UPDATE", "Order " + cl_ord_id + " status: " + std::to_string(static_cast<int>(status)));
            break;
    }
}

void TestStrategy::on_position_update(const proto::PositionUpdate& position) {
    test_results_.position_updates.fetch_add(1);
    
    log_test_event("POSITION_UPDATE",
                   "Exchange: " + position.exch() +
                   ", Symbol: " + position.symbol() +
                   ", Qty: " + std::to_string(position.qty()) +
                   ", Avg Price: " + std::to_string(position.avg_price()));
}

void TestStrategy::on_trade_execution(const proto::Trade& trade) {
    test_results_.trade_executions.fetch_add(1);
    
    log_test_event("TRADE_EXECUTION",
                   "Symbol: " + trade.symbol() +
                   ", Side: " + std::to_string(static_cast<int>(trade.side())) +
                   ", Qty: " + std::to_string(trade.qty()) +
                   ", Price: " + std::to_string(trade.price()));
}

void TestStrategy::on_account_balance_update(const proto::AccountBalanceUpdate& balance_update) {
    test_results_.balance_updates.fetch_add(1);
    
    log_test_event("BALANCE_UPDATE",
                   "Updated " + std::to_string(balance_update.balances_size()) + " balances");
}

void TestStrategy::set_test_scenarios(const std::vector<std::string>& scenarios) {
    test_scenarios_ = scenarios;
}

void TestStrategy::run_basic_order_test() {
    log_test_event("TEST_START", "Basic order test");
    
    std::string order_id = generate_test_order_id();
    
    // Send a buy order
    if (send_test_order(order_id, proto::Side::BUY, proto::OrderType::LIMIT, order_quantity_, order_price_)) {
        test_results_.orders_sent.fetch_add(1);
        log_test_event("ORDER_SENT", "Buy order " + order_id + " sent");
        
        // Wait for ACK
        if (wait_for_order_status(order_id, proto::OrderStatus::NEW)) {
            log_test_event("TEST_PASS", "Basic order test passed");
        } else {
            log_test_event("TEST_FAIL", "Basic order test failed - no ACK received");
        }
    } else {
        log_test_event("TEST_FAIL", "Basic order test failed - order not sent");
    }
}

void TestStrategy::run_partial_fill_test() {
    log_test_event("TEST_START", "Partial fill test");
    
    std::string order_id = generate_test_order_id();
    
    // Send a larger order that should be partially filled
    double large_qty = order_quantity_ * 10;
    
    if (send_test_order(order_id, proto::Side::BUY, proto::OrderType::LIMIT, large_qty, order_price_)) {
        test_results_.orders_sent.fetch_add(1);
        log_test_event("ORDER_SENT", "Large order " + order_id + " sent");
        
        // Wait for partial fill
        if (wait_for_order_status(order_id, proto::OrderStatus::PARTIALLY_FILLED)) {
            log_test_event("TEST_PASS", "Partial fill test passed");
        } else {
            log_test_event("TEST_FAIL", "Partial fill test failed - no partial fill received");
        }
    } else {
        log_test_event("TEST_FAIL", "Partial fill test failed - order not sent");
    }
}

void TestStrategy::run_cancellation_test() {
    log_test_event("TEST_START", "Cancellation test");
    
    std::string order_id = generate_test_order_id();
    
    // Send order
    if (send_test_order(order_id, proto::Side::BUY, proto::OrderType::LIMIT, order_quantity_, order_price_)) {
        test_results_.orders_sent.fetch_add(1);
        
        // Wait for ACK
        if (wait_for_order_status(order_id, proto::OrderStatus::NEW)) {
            // Cancel the order
            if (cancel_test_order(order_id)) {
                log_test_event("CANCEL_SENT", "Cancel request sent for " + order_id);
                
                // Wait for cancellation
                if (wait_for_order_status(order_id, proto::OrderStatus::CANCELLED)) {
                    log_test_event("TEST_PASS", "Cancellation test passed");
                } else {
                    log_test_event("TEST_FAIL", "Cancellation test failed - no cancellation received");
                }
            } else {
                log_test_event("TEST_FAIL", "Cancellation test failed - cancel request not sent");
            }
        } else {
            log_test_event("TEST_FAIL", "Cancellation test failed - no ACK received");
        }
    } else {
        log_test_event("TEST_FAIL", "Cancellation test failed - order not sent");
    }
}

void TestStrategy::run_rejection_test() {
    log_test_event("TEST_START", "Rejection test");
    
    std::string order_id = generate_test_order_id();
    
    // Send an invalid order (very high price to trigger rejection)
    double invalid_price = order_price_ * 1000;
    
    if (send_test_order(order_id, proto::Side::BUY, proto::OrderType::LIMIT, order_quantity_, invalid_price)) {
        test_results_.orders_sent.fetch_add(1);
        log_test_event("ORDER_SENT", "Invalid order " + order_id + " sent");
        
        // Wait for rejection
        if (wait_for_order_status(order_id, proto::OrderStatus::REJECTED)) {
            log_test_event("TEST_PASS", "Rejection test passed");
        } else {
            log_test_event("TEST_FAIL", "Rejection test failed - no rejection received");
        }
    } else {
        log_test_event("TEST_FAIL", "Rejection test failed - order not sent");
    }
}

void TestStrategy::run_market_data_test() {
    log_test_event("TEST_START", "Market data test");
    
    // This test validates that market data is being received
    // The actual validation happens in on_market_data()
    
    // Wait a bit for market data
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    if (test_results_.market_data_received.load() > 0) {
        log_test_event("TEST_PASS", "Market data test passed - received " + 
                      std::to_string(test_results_.market_data_received.load()) + " updates");
    } else {
        log_test_event("TEST_FAIL", "Market data test failed - no market data received");
    }
}

std::string TestStrategy::generate_test_order_id() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "TEST_" << timestamp << "_" << order_counter_.fetch_add(1);
    return ss.str();
}

void TestStrategy::log_test_event(const std::string& event, const std::string& details) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "[TEST_STRATEGY] " << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << " " << event;
    
    if (!details.empty()) {
        std::cout << " - " << details;
    }
    std::cout << std::endl;
}

bool TestStrategy::wait_for_order_status(const std::string& order_id, proto::OrderStatus expected_status, 
                                        std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::system_clock::now();
    
    while (std::chrono::system_clock::now() - start_time < timeout) {
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            auto it = test_results_.order_statuses.find(order_id);
            if (it != test_results_.order_statuses.end() && it->second == expected_status) {
                return true;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

bool TestStrategy::send_test_order(const std::string& cl_ord_id, proto::Side side, proto::OrderType type, 
                                  double qty, double price) {
    // Note: In real implementation, this would delegate to the StrategyContainer
    // For testing, we'll simulate the order sending
    log_test_event("ORDER_SEND_ATTEMPT", 
                   "ID: " + cl_ord_id + 
                   ", Side: " + std::to_string(static_cast<int>(side)) +
                   ", Type: " + std::to_string(static_cast<int>(type)) +
                   ", Qty: " + std::to_string(qty) +
                   ", Price: " + std::to_string(price));
    
    // Simulate successful order submission
    return true;
}

bool TestStrategy::cancel_test_order(const std::string& cl_ord_id) {
    // Note: In real implementation, this would delegate to the StrategyContainer
    log_test_event("CANCEL_SEND_ATTEMPT", "ID: " + cl_ord_id);
    
    // Simulate successful cancel request
    return true;
}

} // namespace testing
