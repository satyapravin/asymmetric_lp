#include <iostream>
#include <memory>
#include "utils/handlers/http/i_http_handler.hpp"
#include "utils/handlers/websocket/i_websocket_handler.hpp"
#include "exch_handler/exchange_handler.hpp"

int main() {
    std::cout << "=== Simple Exchange Handler Example ===" << std::endl;
    
    // Create exchange configuration
    ExchangeConfig config;
    config.name = "BINANCE";
    config.api_key = "your_api_key";
    config.api_secret = "your_api_secret";
    config.base_url = "https://api.binance.com";
    config.websocket_url = "wss://stream.binance.com:9443";
    config.testnet_mode = true;
    
    // Create Binance handler
    auto binance_handler = std::make_unique<BinanceHandler>(config);
    
    // Optionally inject custom handlers for testing
    auto http_handler = HttpHandlerFactory::create("CURL");
    auto websocket_handler = WebSocketHandlerFactory::create("LIBUV");
    
    binance_handler->set_http_handler(std::move(http_handler));
    binance_handler->set_websocket_handler(std::move(websocket_handler));
    
    // Set order event callback
    binance_handler->set_order_event_callback([](const Order& order) {
        std::cout << "Order event: " << order.client_order_id 
                  << " status: " << static_cast<int>(order.status) << std::endl;
    });
    
    // Start the handler
    if (binance_handler->start()) {
        std::cout << "Binance handler started successfully" << std::endl;
        
        // Example: Send an order
        Order test_order;
        test_order.client_order_id = "TEST_ORDER_001";
        test_order.symbol = "BTCUSDT";
        test_order.side = OrderSide::BUY;
        test_order.type = OrderType::LIMIT;
        test_order.quantity = 0.001;
        test_order.price = 50000.0;
        
        if (binance_handler->send_order(test_order)) {
            std::cout << "Order sent successfully" << std::endl;
        } else {
            std::cout << "Failed to send order" << std::endl;
        }
        
        // Stop the handler
        binance_handler->stop();
    } else {
        std::cout << "Failed to start Binance handler" << std::endl;
    }
    
    std::cout << "=== Example completed ===" << std::endl;
    return 0;
}
