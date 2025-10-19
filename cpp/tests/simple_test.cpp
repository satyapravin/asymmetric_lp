#include <iostream>
#include <cassert>
#include "../exchanges/binance/http/binance_oms.hpp"
#include "../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../exchanges/binance/public_websocket/binance_public_websocket_handler.hpp"
#include "../exchanges/binance/private_websocket/binance_private_websocket_handler.hpp"

int main() {
    std::cout << "Testing Binance refactored components..." << std::endl;
    
    try {
        // Test BinanceOMS
        binance::BinanceConfig config;
        config.api_key = "test_key";
        config.api_secret = "test_secret";
        config.base_url = "https://api.binance.com";
        
        binance::BinanceOMS oms(config);
        std::cout << "✓ BinanceOMS created successfully" << std::endl;
        
        // Test connection
        bool connected = oms.connect();
        assert(connected);
        std::cout << "✓ BinanceOMS connected successfully" << std::endl;
        
        // Test authentication
        assert(oms.is_authenticated());
        std::cout << "✓ BinanceOMS authentication working" << std::endl;
        
        // Test BinanceDataFetcher
        binance::BinanceDataFetcher fetcher("test_key", "test_secret");
        std::cout << "✓ BinanceDataFetcher created successfully" << std::endl;
        
        bool fetcher_connected = fetcher.connect("https://api.binance.com");
        assert(fetcher_connected);
        std::cout << "✓ BinanceDataFetcher connected successfully" << std::endl;
        
        // Test BinancePublicWebSocketHandler
        binance::BinancePublicWebSocketHandler public_ws;
        std::cout << "✓ BinancePublicWebSocketHandler created successfully" << std::endl;
        
        bool ws_connected = public_ws.connect("wss://stream.binance.com:9443/ws/btcusdt@depth");
        assert(ws_connected);
        std::cout << "✓ BinancePublicWebSocketHandler connected successfully" << std::endl;
        
        // Test BinancePrivateWebSocketHandler
        binance::BinancePrivateWebSocketHandler private_ws("test_key", "test_secret");
        std::cout << "✓ BinancePrivateWebSocketHandler created successfully" << std::endl;
        
        assert(private_ws.is_authenticated());
        std::cout << "✓ BinancePrivateWebSocketHandler authentication working" << std::endl;
        
        std::cout << "\n🎉 All Binance refactored components working correctly!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
