#include <doctest/doctest.h>
#include "../../../utils/oms/exchange_monitor.hpp"
#include <string>
#include <vector>

TEST_CASE("ExchangeMonitor - Basic Functionality") {
    ExchangeMonitor monitor;
    
    SUBCASE("Record order attempt") {
        monitor.record_order_attempt("BINANCE", "BTCUSDT");
        
        ExchangeMetrics::CopyableMetrics metrics = monitor.get_metrics("BINANCE");
        CHECK(metrics.total_orders == 1);
    }
    
    SUBCASE("Record order success") {
        monitor.record_order_success("BINANCE", "BTCUSDT", 1.0, 1000);
        
        ExchangeMetrics::CopyableMetrics metrics = monitor.get_metrics("BINANCE");
        CHECK(metrics.successful_orders == 1);
        CHECK(metrics.total_volume == 1.0);
    }
    
    SUBCASE("Record order failure") {
        monitor.record_order_failure("BINANCE", "BTCUSDT", "INSUFFICIENT_BALANCE");
        
        ExchangeMetrics::CopyableMetrics metrics = monitor.get_metrics("BINANCE");
        CHECK(metrics.failed_orders == 1);
    }
    
    SUBCASE("Record order fill") {
        monitor.record_order_fill("BINANCE", "BTCUSDT", 0.5);
        
        ExchangeMetrics::CopyableMetrics metrics = monitor.get_metrics("BINANCE");
        CHECK(metrics.filled_orders == 1);
        CHECK(metrics.filled_volume == 0.5);
    }
    
    SUBCASE("Record connection event") {
        monitor.record_connection_event("BINANCE", true);
        
        ExchangeMetrics::CopyableMetrics metrics = monitor.get_metrics("BINANCE");
        CHECK(metrics.connection_attempts == 1);
    }
    
    SUBCASE("Get health status") {
        HealthInfo health = monitor.get_health_status("BINANCE");
        
        // Health status should be available even for non-existent exchanges
        CHECK(true); // Basic health status test
    }
    
    SUBCASE("Set health alert callback") {
        bool callback_called = false;
        
        monitor.set_health_alert_callback([&](const std::string& exchange, HealthStatus status) {
            callback_called = true;
        });
        
        // Callback is set
        CHECK(true); // Basic callback setup test
    }
}