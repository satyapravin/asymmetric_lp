#include "api_endpoint_config.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace exchange_config {

// Global instance
std::unique_ptr<ApiEndpointManager> g_api_endpoint_manager = nullptr;

ApiEndpointManager::ApiEndpointManager() {
    std::cout << "[API_CONFIG] Initializing API endpoint manager" << std::endl;
}

bool ApiEndpointManager::load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[API_CONFIG] Failed to open config file: " << config_file << std::endl;
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(file, root)) {
        std::cerr << "[API_CONFIG] Failed to parse JSON config: " << reader.getFormattedErrorMessages() << std::endl;
        return false;
    }
    
    // Handle production config structure with "exchanges" wrapper
    if (root.isMember("exchanges")) {
        return load_config_from_json(root["exchanges"]);
    }
    
    return load_config_from_json(root);
}

bool ApiEndpointManager::load_config_from_json(const Json::Value& config) {
    try {
        for (const auto& exchange_name : config.getMemberNames()) {
            ExchangeConfig exchange_config = parse_exchange_config(config[exchange_name]);
            exchange_config.exchange_name = exchange_name;
            exchange_configs_[exchange_name] = exchange_config;
            std::cout << "[API_CONFIG] Loaded config for exchange: " << exchange_name << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[API_CONFIG] Error loading config: " << e.what() << std::endl;
        return false;
    }
}

void ApiEndpointManager::set_exchange_config(const std::string& exchange_name, const ExchangeConfig& config) {
    exchange_configs_[exchange_name] = config;
}

ExchangeConfig ApiEndpointManager::get_exchange_config(const std::string& exchange_name) const {
    // Try exact match first
    auto it = exchange_configs_.find(exchange_name);
    if (it != exchange_configs_.end()) {
        return it->second;
    }
    
    // Try case-insensitive match
    std::string lower_name = exchange_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    for (const auto& [key, config] : exchange_configs_) {
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
        if (lower_key == lower_name) {
            return config;
        }
    }
    
    return ExchangeConfig{};
}

bool ApiEndpointManager::has_exchange(const std::string& exchange_name) const {
    return exchange_configs_.find(exchange_name) != exchange_configs_.end();
}

std::string ApiEndpointManager::get_rest_api_url(const std::string& exchange_name, 
                                                AssetType asset_type) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return "";
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it == exchange_config.assets.end()) {
        return "";
    }
    
    const auto& asset_config = asset_it->second;
    return asset_config.urls.rest_api;
}

std::string ApiEndpointManager::get_endpoint_url(const std::string& exchange_name, 
                                                AssetType asset_type, 
                                                const std::string& endpoint_name) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return "";
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it == exchange_config.assets.end()) {
        return "";
    }
    
    const auto& asset_config = asset_it->second;
    auto endpoint_it = asset_config.endpoints.find(endpoint_name);
    if (endpoint_it == asset_config.endpoints.end()) {
        return "";
    }
    
    return build_url(asset_config.urls.rest_api, endpoint_it->second.path);
}

EndpointConfig ApiEndpointManager::get_endpoint_config(const std::string& exchange_name, 
                                                       AssetType asset_type, 
                                                       const std::string& endpoint_name) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return EndpointConfig{};
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it == exchange_config.assets.end()) {
        return EndpointConfig{};
    }
    
    const auto& asset_config = asset_it->second;
    auto endpoint_it = asset_config.endpoints.find(endpoint_name);
    if (endpoint_it == asset_config.endpoints.end()) {
        return EndpointConfig{};
    }
    
    return endpoint_it->second;
}

std::string ApiEndpointManager::get_websocket_url(const std::string& exchange_name, 
                                                  AssetType asset_type, 
                                                  const std::string& websocket_type) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return "";
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it == exchange_config.assets.end()) {
        return "";
    }
    
    const auto& asset_config = asset_it->second;
    
    if (websocket_type == "public") {
        return asset_config.urls.websocket_public;
    } else if (websocket_type == "private") {
        return asset_config.urls.websocket_private;
    } else if (websocket_type == "market_data") {
        return asset_config.urls.websocket_market_data;
    } else if (websocket_type == "user_data") {
        return asset_config.urls.websocket_user_data;
    }
    
    return "";
}

std::string ApiEndpointManager::get_websocket_channel_name(const std::string& exchange_name, 
                                                          AssetType asset_type, 
                                                          const std::string& channel_type) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return "";
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it == exchange_config.assets.end()) {
        return "";
    }
    
    const auto& asset_config = asset_it->second;
    
    if (channel_type == "orderbook") {
        return asset_config.websocket_channels.orderbook;
    } else if (channel_type == "trades") {
        return asset_config.websocket_channels.trades;
    } else if (channel_type == "ticker") {
        return asset_config.websocket_channels.ticker;
    } else if (channel_type == "user_data") {
        return asset_config.websocket_channels.user_data;
    }
    
    return "";
}

AuthConfig ApiEndpointManager::get_authentication_config(const std::string& exchange_name) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return AuthConfig{};
    }
    
    return exchange_config.authentication;
}

AssetConfig ApiEndpointManager::get_asset_config(const std::string& exchange_name, AssetType asset_type) const {
    auto exchange_config = get_exchange_config(exchange_name);
    if (exchange_config.exchange_name.empty()) {
        return AssetConfig{};
    }
    
    auto asset_it = exchange_config.assets.find(asset_type);
    if (asset_it != exchange_config.assets.end()) {
        return asset_it->second;
    }
    
    return AssetConfig{};
}

std::vector<AssetType> ApiEndpointManager::get_supported_assets(const std::string& exchange_name) const {
    std::vector<AssetType> assets;
    auto exchange_config = get_exchange_config(exchange_name);
    
    for (const auto& [asset_type, asset_config] : exchange_config.assets) {
        assets.push_back(asset_type);
    }
    
    return assets;
}

std::string ApiEndpointManager::build_url(const std::string& base_url, 
                                         const std::string& path,
                                         const std::map<std::string, std::string>& params) const {
    std::string url = base_url;
    if (!path.empty()) {
        if (url.back() != '/' && !path.empty() && path.front() != '/') {
            url += '/';
        }
        url += path;
    }
    
    if (!params.empty()) {
        url += "?" + build_query_string(params);
    }
    
    return url;
}

std::string ApiEndpointManager::build_websocket_url(const std::string& base_url,
                                                   const std::string& path,
                                                   const std::map<std::string, std::string>& params) const {
    std::string url = base_url;
    if (!path.empty()) {
        if (url.back() != '/' && !path.empty() && path.front() != '/') {
            url += '/';
        }
        url += path;
    }
    
    if (!params.empty()) {
        url += "?" + build_query_string(params);
    }
    
    return url;
}

bool ApiEndpointManager::validate_config(const ExchangeConfig& config) {
    validation_errors_.clear();
    
    if (config.exchange_name.empty()) {
        validation_errors_.push_back("Exchange name is required");
    }
    
    if (config.assets.empty()) {
        validation_errors_.push_back("At least one asset type must be configured");
    }
    
    for (const auto& [asset_type, asset_config] : config.assets) {
        if (asset_config.urls.rest_api.empty()) {
            validation_errors_.push_back("Base URL is required for asset type: " + asset_type_to_string(asset_type));
        }
        
        if (asset_config.endpoints.empty()) {
            validation_errors_.push_back("At least one endpoint must be configured for asset type: " + asset_type_to_string(asset_type));
        }
    }
    
    return validation_errors_.empty();
}

std::vector<std::string> ApiEndpointManager::get_validation_errors() const {
    return validation_errors_;
}

void ApiEndpointManager::load_default_binance_config() {
    // Load from JSON configuration file instead of hardcoded values
    std::string config_file = "cpp/exchanges/config/api_endpoints.json";
    if (!load_config(config_file)) {
        std::cerr << "[API_CONFIG] Failed to load Binance config from JSON, using fallback" << std::endl;
        // Fallback to minimal configuration
        ExchangeConfig config;
        config.exchange_name = "BINANCE";
        config.version = "v1";
        config.default_timeout_ms = 5000;
        config.max_retries = 3;
        exchange_configs_["BINANCE"] = config;
    }
}

void ApiEndpointManager::load_default_deribit_config() {
    // Load from JSON configuration file instead of hardcoded values
    std::string config_file = "cpp/exchanges/config/api_endpoints.json";
    if (!load_config(config_file)) {
        std::cerr << "[API_CONFIG] Failed to load Deribit config from JSON, using fallback" << std::endl;
        // Fallback to minimal configuration
        ExchangeConfig config;
        config.exchange_name = "DERIBIT";
        config.version = "v2";
        config.default_timeout_ms = 5000;
        config.max_retries = 3;
        exchange_configs_["DERIBIT"] = config;
    }
}

void ApiEndpointManager::load_default_grvt_config() {
    // Load from JSON configuration file instead of hardcoded values
    std::string config_file = "cpp/exchanges/config/api_endpoints.json";
    if (!load_config(config_file)) {
        std::cerr << "[API_CONFIG] Failed to load GRVT config from JSON, using fallback" << std::endl;
        // Fallback to minimal configuration
        ExchangeConfig config;
        config.exchange_name = "GRVT";
        config.version = "v1";
        config.default_timeout_ms = 5000;
        config.max_retries = 3;
        exchange_configs_["GRVT"] = config;
    }
}

std::string ApiEndpointManager::asset_type_to_string(AssetType type) {
    switch (type) {
        case AssetType::SPOT: return "spot";
        case AssetType::FUTURES: return "futures";
        case AssetType::OPTIONS: return "options";
        case AssetType::MARGIN: return "margin";
        case AssetType::PERPETUAL: return "perpetual";
        default: return "unknown";
    }
}

AssetType ApiEndpointManager::string_to_asset_type(const std::string& type) {
    std::string lower_type = type;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);
    
    if (lower_type == "spot") return AssetType::SPOT;
    if (lower_type == "futures") return AssetType::FUTURES;
    if (lower_type == "options") return AssetType::OPTIONS;
    if (lower_type == "margin") return AssetType::MARGIN;
    if (lower_type == "perpetual") return AssetType::PERPETUAL;
    return AssetType::SPOT; // Default
}

std::string ApiEndpointManager::http_method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        default: return "GET";
    }
}

HttpMethod ApiEndpointManager::string_to_http_method(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "PATCH") return HttpMethod::PATCH;
    return HttpMethod::GET; // Default
}

ExchangeConfig ApiEndpointManager::parse_exchange_config(const Json::Value& json) const {
    ExchangeConfig config;
    config.version = json.get("version", "v1").asString();
    config.default_timeout_ms = json.get("default_timeout_ms", 5000).asInt();
    config.max_retries = json.get("max_retries", 3).asInt();
    config.testnet_mode = json.get("testnet_mode", false).asBool();
    
    // Parse global headers and params
    if (json.isMember("global_headers")) {
        for (const auto& header_name : json["global_headers"].getMemberNames()) {
            config.global_headers[header_name] = json["global_headers"][header_name].asString();
        }
    }
    
    if (json.isMember("global_params")) {
        for (const auto& param_name : json["global_params"].getMemberNames()) {
            config.global_params[param_name] = json["global_params"][param_name].asString();
        }
    }
    
    // Parse asset configurations
    if (json.isMember("assets")) {
        for (const auto& asset_name : json["assets"].getMemberNames()) {
            AssetType asset_type = string_to_asset_type(asset_name);
            AssetConfig asset_config = parse_asset_config(json["assets"][asset_name]);
            asset_config.type = asset_type;
            asset_config.name = asset_name;
            config.assets[asset_type] = asset_config;
        }
    }
    
    // Parse authentication configuration
    if (json.isMember("authentication")) {
        const Json::Value& auth = json["authentication"];
        config.authentication.api_key_header = auth.get("api_key_header", "").asString();
        config.authentication.signature_param = auth.get("signature_param", "").asString();
        config.authentication.timestamp_param = auth.get("timestamp_param", "").asString();
        config.authentication.session_cookie = auth.get("session_cookie", "").asString();
        config.authentication.account_id_header = auth.get("account_id_header", "").asString();
        config.authentication.client_id = auth.get("client_id", "").asString();
        config.authentication.client_secret = auth.get("client_secret", "").asString();
        config.authentication.grant_type = auth.get("grant_type", "").asString();
    }
    
    return config;
}

AssetConfig ApiEndpointManager::parse_asset_config(const Json::Value& json) const {
    AssetConfig config;
    
    // Parse URLs - they can be either directly in the asset config or nested under "urls"
    if (json.isMember("urls")) {
        const Json::Value& urls = json["urls"];
        config.urls.rest_api = urls.get("rest_api", "").asString();
        config.urls.websocket_public = urls.get("websocket_public", "").asString();
        config.urls.websocket_private = urls.get("websocket_private", "").asString();
        config.urls.websocket_market_data = urls.get("websocket_market_data", "").asString();
        config.urls.websocket_user_data = urls.get("websocket_user_data", "").asString();
    } else {
        // Fallback to direct fields for backward compatibility
        config.urls.rest_api = json.get("rest_api", "").asString();
        config.urls.websocket_public = json.get("websocket_public", "").asString();
        config.urls.websocket_private = json.get("websocket_private", "").asString();
        config.urls.websocket_market_data = json.get("websocket_market_data", "").asString();
        config.urls.websocket_user_data = json.get("websocket_user_data", "").asString();
    }
    
    // Parse websocket channels
    if (json.isMember("websocket_channels")) {
        const Json::Value& channels = json["websocket_channels"];
        config.websocket_channels.orderbook = channels.get("orderbook", "").asString();
        config.websocket_channels.trades = channels.get("trades", "").asString();
        config.websocket_channels.ticker = channels.get("ticker", "").asString();
        config.websocket_channels.user_data = channels.get("user_data", "").asString();
    }
    
    // Parse endpoints
    if (json.isMember("endpoints")) {
        for (const auto& endpoint_name : json["endpoints"].getMemberNames()) {
            EndpointConfig endpoint_config = parse_endpoint_config(json["endpoints"][endpoint_name]);
            endpoint_config.name = endpoint_name;
            config.endpoints[endpoint_name] = endpoint_config;
        }
    }
    
    // Parse default headers and params
    if (json.isMember("headers")) {
        for (const auto& header_name : json["headers"].getMemberNames()) {
            config.headers[header_name] = json["headers"][header_name].asString();
        }
    }
    
    if (json.isMember("params")) {
        for (const auto& param_name : json["params"].getMemberNames()) {
            config.params[param_name] = json["params"][param_name].asString();
        }
    }
    
    return config;
}

EndpointConfig ApiEndpointManager::parse_endpoint_config(const Json::Value& json) const {
    EndpointConfig config;
    config.path = json.get("path", "").asString();
    config.method = string_to_http_method(json.get("method", "GET").asString());
    config.requires_auth = json.get("requires_auth", false).asBool();
    config.requires_signature = json.get("requires_signature", false).asBool();
    config.description = json.get("description", "").asString();
    
    // Parse default parameters
    if (json.isMember("default_params")) {
        for (const auto& param_name : json["default_params"].getMemberNames()) {
            config.default_params[param_name] = json["default_params"][param_name].asString();
        }
    }
    
    return config;
}

std::string ApiEndpointManager::url_encode(const std::string& str) const {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }
    
    return escaped.str();
}

std::string ApiEndpointManager::build_query_string(const std::map<std::string, std::string>& params) const {
    std::ostringstream query;
    bool first = true;
    
    for (const auto& [key, value] : params) {
        if (!first) {
            query << "&";
        }
        query << url_encode(key) << "=" << url_encode(value);
        first = false;
    }
    
    return query.str();
}

// Convenience functions
void initialize_api_endpoint_manager() {
    if (!g_api_endpoint_manager) {
        g_api_endpoint_manager = std::make_unique<ApiEndpointManager>();
        g_api_endpoint_manager->load_default_binance_config();
        g_api_endpoint_manager->load_default_deribit_config();
        g_api_endpoint_manager->load_default_grvt_config();
    }
}

void load_exchange_configs() {
    initialize_api_endpoint_manager();
}

std::string get_api_endpoint(const std::string& exchange_name, 
                           AssetType asset_type, 
                           const std::string& endpoint_name) {
    if (!g_api_endpoint_manager) {
        initialize_api_endpoint_manager();
    }
    return g_api_endpoint_manager->get_endpoint_url(exchange_name, asset_type, endpoint_name);
}

EndpointConfig get_endpoint_info(const std::string& exchange_name,
                                AssetType asset_type,
                                const std::string& endpoint_name) {
    if (!g_api_endpoint_manager) {
        initialize_api_endpoint_manager();
    }
    return g_api_endpoint_manager->get_endpoint_config(exchange_name, asset_type, endpoint_name);
}

} // namespace exchange_config
