#include "binance_oms.hpp"
#include "../../../utils/logging/log_helper.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <ctime>
#include <cstdio>
#include <curl/curl.h>
#include <json/json.h>

namespace binance {

// CURL global state management with reference counting
namespace {
    std::mutex curl_init_mutex;
    std::atomic<int> curl_ref_count{0};
    
    void ensure_curl_initialized() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_add(1) == 0) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
    }
    
    void ensure_curl_cleanup() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_sub(1) == 1) {
            curl_global_cleanup();
        }
    }
}

// HTTP response callback for CURL
static size_t OMSWriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch (std::bad_alloc& e) {
        return 0;
    }
}

BinanceOMS::BinanceOMS(const BinanceConfig& config) 
    : config_(config), connected_(false), authenticated_(false) {
    LOG_INFO_COMP("BINANCE", "Initializing Binance OMS");
    
    // Initialize CURL with reference counting
    ensure_curl_initialized();
}

BinanceOMS::~BinanceOMS() {
    LOG_INFO_COMP("BINANCE", "Destroying Binance OMS");
    // Cleanup CURL with reference counting
    ensure_curl_cleanup();
}

bool BinanceOMS::connect() {
    if (custom_transport_) {
        // If a custom transport is injected (for testing)
        LOG_INFO_COMP("BINANCE", "Connected using injected transport");
        connected_.store(true);
        authenticated_.store(true); // Assume authenticated for mock transport
        return true;
    }
    
    if (!is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Cannot connect without authentication");
        return false;
    }
    
    connected_.store(true);
    LOG_INFO_COMP("BINANCE", "Connected to Binance");
    return true;
}

void BinanceOMS::disconnect() {
    connected_.store(false);
    LOG_INFO_COMP("BINANCE", "Disconnected from Binance");
}

bool BinanceOMS::is_connected() const {
    return connected_.load();
}

void BinanceOMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    config_.api_secret = secret;
    authenticated_.store(!api_key.empty() && !secret.empty());
}

bool BinanceOMS::is_authenticated() const {
    return authenticated_.load();
}

bool BinanceOMS::cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Not connected or authenticated");
        return false;
    }
    
    std::string endpoint = "/fapi/v1/order";
    std::string params = "symbol=BTCUSDT&orderId=" + exch_ord_id;
    
    std::string response = make_request(endpoint, "DELETE", params, true);
    if (response.empty()) {
        LOG_ERROR_COMP("BINANCE", "Failed to cancel order");
        return false;
    }
    
    // Parse response to check if successful
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response, root)) {
        std::string status = root["status"].asString();
        return status == "CANCELED";
    }
    
    return false;
}

bool BinanceOMS::replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Not connected or authenticated");
        return false;
    }
    
    // First cancel the existing order
    if (!cancel_order(cl_ord_id, "")) {  // We don't have exch_order_id in OrderRequest
        LOG_ERROR_COMP("BINANCE", "Failed to cancel existing order for replace");
        return false;
    }
    
    // Then place the new order
    if (new_order.type() == proto::OrderType::MARKET) {
        return place_market_order(new_order.symbol(), 
                                 new_order.side() == proto::Side::BUY ? "BUY" : "SELL", 
                                 new_order.qty());
    } else if (new_order.type() == proto::OrderType::LIMIT) {
        return place_limit_order(new_order.symbol(), 
                                new_order.side() == proto::Side::BUY ? "BUY" : "SELL", 
                                new_order.qty(), 
                                new_order.price());
    }
    
    return false;
}

proto::OrderEvent BinanceOMS::get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) {
    proto::OrderEvent order_event;
    
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Not connected or authenticated");
        order_event.set_text("Not connected or authenticated");
        return order_event;
    }
    
    std::string endpoint = "/fapi/v1/order";
    std::string params = "symbol=BTCUSDT&orderId=" + exch_ord_id;
    
    std::string response = make_request(endpoint, "GET", params, true);
    if (response.empty()) {
        LOG_ERROR_COMP("BINANCE", "Failed to get order status");
        order_event.set_text("Failed to get order status");
        return order_event;
    }
    
    return parse_order_from_json(response);
}

bool BinanceOMS::place_market_order(const std::string& symbol, const std::string& side, double quantity) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Not connected or authenticated");
        return false;
    }
    
    std::string endpoint = "/fapi/v1/order";
    std::string params = "symbol=" + symbol + 
                        "&side=" + side + 
                        "&type=MARKET" + 
                        "&quantity=" + std::to_string(quantity);
    
    std::string response = make_request(endpoint, "POST", params, true);
    if (response.empty()) {
        LOG_ERROR_COMP("BINANCE", "Failed to place market order");
        return false;
    }
    
    // Parse response to check if successful
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response, root)) {
        std::string status = root["status"].asString();
        return status == "NEW" || status == "FILLED";
    }
    
    return false;
}

bool BinanceOMS::place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) {
    if (!is_connected() || !is_authenticated()) {
        LOG_ERROR_COMP("BINANCE", "Not connected or authenticated");
        return false;
    }
    
    std::string endpoint = "/fapi/v1/order";
    std::string params = "symbol=" + symbol + 
                        "&side=" + side + 
                        "&type=LIMIT" + 
                        "&quantity=" + std::to_string(quantity) +
                        "&price=" + std::to_string(price) +
                        "&timeInForce=GTC";
    
    std::string response = make_request(endpoint, "POST", params, true);
    if (response.empty()) {
        LOG_ERROR_COMP("BINANCE", "Failed to place limit order");
        return false;
    }
    
    // Parse response to check if successful
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response, root)) {
        std::string status = root["status"].asString();
        return status == "NEW" || status == "FILLED";
    }
    
    return false;
}

void BinanceOMS::set_order_status_callback(OrderStatusCallback callback) {
    order_callback_ = callback;
}

std::string BinanceOMS::make_request(const std::string& endpoint, const std::string& method, 
                                   const std::string& body, bool is_signed) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR_COMP("BINANCE", "Failed to initialize CURL");
        return "";
    }
    
    std::string url = config_.base_url + endpoint;
    if (!body.empty()) {
        url += "?" + body;
    }
    
    // Add timestamp and signature for signed requests
    if (is_signed) {
        std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        std::string query_string = body.empty() ? "" : body + "&";
        query_string += "timestamp=" + timestamp;
        
        std::string signature = generate_signature(query_string);
        query_string += "&signature=" + signature;
        
        url = config_.base_url + endpoint + "?" + query_string;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OMSWriteCallback);
    
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    // Add API key header
    struct curl_slist* headers = nullptr;
    std::string api_key_header = "X-MBX-APIKEY: " + config_.api_key;
    headers = curl_slist_append(headers, api_key_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR_COMP("BINANCE", "CURL error: " + std::string(curl_easy_strerror(res)));
        return "";
    }
    
    return response_data;
}

std::string BinanceOMS::generate_signature(const std::string& data) {
    unsigned char* digest = HMAC(EVP_sha256(), 
                                 config_.api_secret.c_str(), config_.api_secret.length(),
                                 (unsigned char*)data.c_str(), data.length(),
                                 nullptr, nullptr);
    
    char md_string[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&md_string[i*2], "%02x", (unsigned int)digest[i]);
    }
    
    return std::string(md_string);
}

std::string BinanceOMS::create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) {
    // This method is not used in the simplified implementation
    return "";
}

std::string BinanceOMS::order_side_to_string(const std::string& side) {
    return side; // Already a string
}

std::string BinanceOMS::order_type_to_string(const std::string& type) {
    return type; // Already a string
}

proto::OrderEvent BinanceOMS::parse_order_from_json(const std::string& json_str) {
    proto::OrderEvent order_event;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_str, root)) {
        LOG_ERROR_COMP("BINANCE", "Failed to parse order JSON");
        order_event.set_text("Failed to parse order JSON");
        return order_event;
    }
    
    order_event.set_cl_ord_id(root["clientOrderId"].asString());
    order_event.set_exch("binance");
    order_event.set_symbol(root["symbol"].asString());
    order_event.set_exch_order_id(root["orderId"].asString());
    order_event.set_fill_qty(std::stod(root["executedQty"].asString()));
    order_event.set_fill_price(std::stod(root["avgPrice"].asString()));
    order_event.set_timestamp_us(root["time"].asUInt64() * 1000); // Convert to microseconds
    
    // Map Binance order status to OrderEventType
    std::string status = root["status"].asString();
    if (status == "NEW") {
        order_event.set_event_type(proto::OrderEventType::ACK);
    } else if (status == "FILLED") {
        order_event.set_event_type(proto::OrderEventType::FILL);
    } else if (status == "CANCELED") {
        order_event.set_event_type(proto::OrderEventType::CANCEL);
    } else if (status == "REJECTED") {
        order_event.set_event_type(proto::OrderEventType::REJECT);
    } else {
        order_event.set_event_type(proto::OrderEventType::ACK);  // Default to ACK
    }
    
    return order_event;
}

std::string BinanceOMS::get_error_message(const std::string& response) {
    Json::Value root;
    Json::Reader reader;
    
    if (reader.parse(response, root)) {
        if (root.isMember("msg")) {
            return root["msg"].asString();
        }
        if (root.isMember("error")) {
            return root["error"].asString();
        }
    }
    
    return "Unknown error";
}

void BinanceOMS::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    custom_transport_ = transport;
    if (!custom_transport_) return;
    
    custom_transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& msg) {
        try {
            proto::OrderEvent evt = parse_order_from_json(msg.data);
            if (order_callback_) order_callback_(evt);
        } catch (...) {
            // swallow parsing errors in tests
        }
    });
}

} // namespace binance