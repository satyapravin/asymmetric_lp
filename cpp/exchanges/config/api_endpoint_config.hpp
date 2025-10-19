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

// URL configuration for different protocols
struct UrlConfig {
    std::string rest_api;              // HTTP REST API base URL
    std::string websocket_public;      // Public WebSocket URL
    std::string websocket_private;     // Private WebSocket URL  
    std::string websocket_market_data; // Market data WebSocket URL
    std::string websocket_user_data;   // User data WebSocket URL
};

// WebSocket channel mapping
struct WebSocketChannels {
    std::string orderbook;    // Orderbook channel name
    std::string trades;       // Trades channel name
    std::string ticker;       // Ticker channel name
    std::string user_data;    // User data channel name
};

// Authentication configuration
struct AuthConfig {
    std::string api_key_header;       // Header name for API key
    std::string signature_param;      // Parameter name for signature
    std::string timestamp_param;      // Parameter name for timestamp
    std::string session_cookie;       // Session cookie name (GRVT)
    std::string account_id_header;    // Account ID header (GRVT)
    std::string client_id;            // Client ID (Deribit)
    std::string client_secret;        // Client secret (Deribit)
    std::string grant_type;           // OAuth grant type (Deribit)
};

// Asset-specific configuration
struct AssetConfig {
    AssetType type;
    std::string name;           // "spot", "futures", "perpetual"
    UrlConfig urls;             // All URL types for this asset
    std::map<std::string, EndpointConfig> endpoints;  // REST API endpoints
    WebSocketChannels websocket_channels;  // WebSocket channel names
    std::map<std::string, std::string> headers;       // Default headers
    std::map<std::string, std::string> params;         // Default parameters
};

// Exchange configuration
struct ExchangeConfig {
    std::string exchange_name;
    std::string version;        // API version
    std::map<AssetType, AssetConfig> assets;  // Asset type configurations
    AuthConfig authentication;  // Authentication configuration
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
    
    // URL resolution for different protocols
    std::string get_rest_api_url(const std::string& exchange_name, AssetType asset_type) const;
    std::string get_endpoint_url(const std::string& exchange_name, AssetType asset_type, const std::string& endpoint_name) const;
    EndpointConfig get_endpoint_config(const std::string& exchange_name, AssetType asset_type, const std::string& endpoint_name) const;
    std::string get_websocket_url(const std::string& exchange_name, AssetType asset_type, 
                                 const std::string& websocket_type = "public") const;
    std::string get_websocket_channel_name(const std::string& exchange_name, AssetType asset_type,
                                          const std::string& channel_type) const;
    
    // Authentication configuration
    AuthConfig get_authentication_config(const std::string& exchange_name) const;
    
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
    bool validate_config(const ExchangeConfig& config);
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
