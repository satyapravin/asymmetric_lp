#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <iostream>
#include <fstream>
#include <json/json.h>

namespace exchange_config {

// Asset types supported by exchanges
enum class AssetType {
    SPOT,           // Spot trading
    FUTURES,        // Futures trading
    OPTIONS,        // Options trading
    MARGIN,         // Margin trading
    PERPETUAL       // Perpetual futures
};

// API endpoint types
enum class EndpointType {
    REST_API,       // REST API endpoints
    WEBSOCKET_PUBLIC,   // Public WebSocket streams
    WEBSOCKET_PRIVATE,  // Private WebSocket streams
    WEBSOCKET_MARKET_DATA,  // Market data streams
    WEBSOCKET_USER_DATA     // User data streams
};

// HTTP methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
};

// Endpoint configuration
struct EndpointConfig {
    std::string name;           // Endpoint name (e.g., "place_order")
    std::string path;           // API path (e.g., "/fapi/v1/order")
    HttpMethod method;          // HTTP method
    bool requires_auth;         // Requires authentication
    bool requires_signature;    // Requires signature
    std::map<std::string, std::string> default_params;  // Default parameters
    std::string description;   // Human readable description
};

// Asset-specific configuration
struct AssetConfig {
    AssetType type;
    std::string name;           // "spot", "futures", "perpetual"
    std::string base_url;       // Base URL for this asset type
    std::string ws_url;         // WebSocket URL for this asset type
    std::map<std::string, EndpointConfig> endpoints;  // Endpoints for this asset type
    std::map<std::string, std::string> headers;       // Default headers
    std::map<std::string, std::string> params;         // Default parameters
};

// Exchange configuration
struct ExchangeConfig {
    std::string exchange_name;
    std::string version;        // API version
    std::map<AssetType, AssetConfig> assets;  // Asset type configurations
    std::map<std::string, std::string> global_headers;  // Global headers
    std::map<std::string, std::string> global_params;  // Global parameters
    int default_timeout_ms{5000};
    int max_retries{3};
    bool testnet_mode{false};
};

// API endpoint manager
class ApiEndpointManager {
public:
    ApiEndpointManager();
    ~ApiEndpointManager() = default;
    
    // Configuration loading
    bool load_config(const std::string& config_file);
    bool load_config_from_json(const Json::Value& config);
    void save_config(const std::string& config_file) const;
    
    // Exchange configuration management
    void set_exchange_config(const std::string& exchange_name, const ExchangeConfig& config);
    ExchangeConfig get_exchange_config(const std::string& exchange_name) const;
    bool has_exchange(const std::string& exchange_name) const;
    
    // Endpoint resolution
    std::string get_endpoint_url(const std::string& exchange_name, 
                                AssetType asset_type, 
                                const std::string& endpoint_name) const;
    
    EndpointConfig get_endpoint_config(const std::string& exchange_name,
                                     AssetType asset_type,
                                     const std::string& endpoint_name) const;
    
    // Asset type management
    AssetConfig get_asset_config(const std::string& exchange_name, AssetType asset_type) const;
    std::vector<AssetType> get_supported_assets(const std::string& exchange_name) const;
    
    // URL construction helpers
    std::string build_url(const std::string& base_url, 
                         const std::string& path,
                         const std::map<std::string, std::string>& params = {}) const;
    
    std::string build_websocket_url(const std::string& base_url,
                                   const std::string& path,
                                   const std::map<std::string, std::string>& params = {}) const;
    
    // Configuration validation
    bool validate_config(const ExchangeConfig& config) const;
    std::vector<std::string> get_validation_errors() const;
    
    // Default configurations
    void load_default_binance_config();
    void load_default_deribit_config();
    void load_default_grvt_config();
    
    // Utility methods
    static std::string asset_type_to_string(AssetType type);
    static AssetType string_to_asset_type(const std::string& type);
    static std::string http_method_to_string(HttpMethod method);
    static HttpMethod string_to_http_method(const std::string& method);

private:
    std::map<std::string, ExchangeConfig> exchange_configs_;
    std::vector<std::string> validation_errors_;
    
    // Configuration parsing helpers
    ExchangeConfig parse_exchange_config(const Json::Value& json) const;
    AssetConfig parse_asset_config(const Json::Value& json) const;
    EndpointConfig parse_endpoint_config(const Json::Value& json) const;
    
    // URL building helpers
    std::string url_encode(const std::string& str) const;
    std::string build_query_string(const std::map<std::string, std::string>& params) const;
};

// Global instance for easy access
extern std::unique_ptr<ApiEndpointManager> g_api_endpoint_manager;

// Convenience functions
void initialize_api_endpoint_manager();
void load_exchange_configs();
std::string get_api_endpoint(const std::string& exchange_name, 
                           AssetType asset_type, 
                           const std::string& endpoint_name);
EndpointConfig get_endpoint_info(const std::string& exchange_name,
                                AssetType asset_type,
                                const std::string& endpoint_name);

} // namespace exchange_config
