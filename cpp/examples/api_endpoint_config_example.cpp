#include <iostream>
#include <memory>
#include "exchanges/binance/binance_oms.hpp"
#include "exchanges/config/api_endpoint_config.hpp"

int main() {
    std::cout << "=== API Endpoint Configuration Example ===" << std::endl;
    
    // Initialize API endpoint manager
    exchange_config::initialize_api_endpoint_manager();
    
    // Example 1: Using different asset types
    std::cout << "\n=== Example 1: Different Asset Types ===" << std::endl;
    
    // Futures trading
    binance::BinanceConfig futures_config;
    futures_config.api_key = "your_api_key";
    futures_config.api_secret = "your_api_secret";
    futures_config.asset_type = exchange_config::AssetType::FUTURES;
    
    auto futures_oms = std::make_unique<binance::BinanceOMS>(futures_config);
    std::cout << "Futures asset type: " 
              << exchange_config::ApiEndpointManager::asset_type_to_string(futures_oms->get_asset_type()) << std::endl;
    
    // Get futures endpoint
    std::string futures_endpoint = futures_oms->get_endpoint_url("place_order");
    std::cout << "Futures place order endpoint: " << futures_endpoint << std::endl;
    
    // Spot trading
    binance::BinanceConfig spot_config;
    spot_config.api_key = "your_api_key";
    spot_config.api_secret = "your_api_secret";
    spot_config.asset_type = exchange_config::AssetType::SPOT;
    
    auto spot_oms = std::make_unique<binance::BinanceOMS>(spot_config);
    std::cout << "Spot asset type: " 
              << exchange_config::ApiEndpointManager::asset_type_to_string(spot_oms->get_asset_type()) << std::endl;
    
    // Get spot endpoint
    std::string spot_endpoint = spot_oms->get_endpoint_url("place_order");
    std::cout << "Spot place order endpoint: " << spot_endpoint << std::endl;
    
    // Example 2: Dynamic asset type switching
    std::cout << "\n=== Example 2: Dynamic Asset Type Switching ===" << std::endl;
    
    auto oms = std::make_unique<binance::BinanceOMS>(futures_config);
    
    // Switch to spot
    oms->set_asset_type(exchange_config::AssetType::SPOT);
    std::cout << "Switched to: " 
              << exchange_config::ApiEndpointManager::asset_type_to_string(oms->get_asset_type()) << std::endl;
    std::cout << "Spot endpoint: " << oms->get_endpoint_url("place_order") << std::endl;
    
    // Switch back to futures
    oms->set_asset_type(exchange_config::AssetType::FUTURES);
    std::cout << "Switched to: " 
              << exchange_config::ApiEndpointManager::asset_type_to_string(oms->get_asset_type()) << std::endl;
    std::cout << "Futures endpoint: " << oms->get_endpoint_url("place_order") << std::endl;
    
    // Example 3: Endpoint configuration details
    std::cout << "\n=== Example 3: Endpoint Configuration Details ===" << std::endl;
    
    auto endpoint_config = oms->get_endpoint_config("place_order");
    std::cout << "Place order endpoint details:" << std::endl;
    std::cout << "  Path: " << endpoint_config.path << std::endl;
    std::cout << "  Method: " << exchange_config::ApiEndpointManager::http_method_to_string(endpoint_config.method) << std::endl;
    std::cout << "  Requires auth: " << (endpoint_config.requires_auth ? "Yes" : "No") << std::endl;
    std::cout << "  Requires signature: " << (endpoint_config.requires_signature ? "Yes" : "No") << std::endl;
    std::cout << "  Description: " << endpoint_config.description << std::endl;
    
    // Example 4: Different exchanges
    std::cout << "\n=== Example 4: Different Exchanges ===" << std::endl;
    
    // Binance futures
    std::string binance_futures_endpoint = exchange_config::get_api_endpoint("BINANCE", exchange_config::AssetType::FUTURES, "place_order");
    std::cout << "Binance futures endpoint: " << binance_futures_endpoint << std::endl;
    
    // Deribit options
    std::string deribit_options_endpoint = exchange_config::get_api_endpoint("DERIBIT", exchange_config::AssetType::OPTIONS, "place_order");
    std::cout << "Deribit options endpoint: " << deribit_options_endpoint << std::endl;
    
    // GRVT perpetual
    std::string grvt_perpetual_endpoint = exchange_config::get_api_endpoint("GRVT", exchange_config::AssetType::PERPETUAL, "place_order");
    std::cout << "GRVT perpetual endpoint: " << grvt_perpetual_endpoint << std::endl;
    
    // Example 5: Asset configuration
    std::cout << "\n=== Example 5: Asset Configuration ===" << std::endl;
    
    auto asset_config = oms->get_asset_config();
    std::cout << "Current asset configuration:" << std::endl;
    std::cout << "  Type: " << exchange_config::ApiEndpointManager::asset_type_to_string(asset_config.type) << std::endl;
    std::cout << "  Name: " << asset_config.name << std::endl;
    std::cout << "  Base URL: " << asset_config.base_url << std::endl;
    std::cout << "  WebSocket URL: " << asset_config.ws_url << std::endl;
    std::cout << "  Available endpoints: " << asset_config.endpoints.size() << std::endl;
    
    for (const auto& [endpoint_name, endpoint_config] : asset_config.endpoints) {
        std::cout << "    - " << endpoint_name << ": " << endpoint_config.path << std::endl;
    }
    
    std::cout << "\n=== Example completed ===" << std::endl;
    return 0;
}
