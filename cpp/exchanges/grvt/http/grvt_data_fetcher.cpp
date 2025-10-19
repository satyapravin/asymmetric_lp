#include "grvt_data_fetcher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <json/json.h>

namespace grvt {

GrvtDataFetcher::GrvtDataFetcher(const std::string& api_key, const std::string& session_cookie, const std::string& account_id)
    : curl_(nullptr), authenticated_(false) {
    config_.api_key = api_key;
    config_.session_cookie = session_cookie;
    config_.account_id = account_id;
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to initialize CURL" << std::endl;
    }
}

GrvtDataFetcher::~GrvtDataFetcher() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void GrvtDataFetcher::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    // Note: GRVT uses session cookies instead of secrets
    authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
}

bool GrvtDataFetcher::is_authenticated() const {
    return authenticated_.load();
}

std::vector<proto::OrderEvent> GrvtDataFetcher::get_open_orders() {
    if (!is_authenticated()) {
        std::cerr << "[GRVT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("orders", R"({"status":"open"})");
    if (response.empty()) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to fetch open orders" << std::endl;
        return {};
    }
    
    return parse_orders(response);
}

std::vector<proto::OrderEvent> GrvtDataFetcher::get_order_history() {
    if (!is_authenticated()) {
        std::cerr << "[GRVT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("orders", R"({"status":"all"})");
    if (response.empty()) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to fetch order history" << std::endl;
        return {};
    }
    
    return parse_orders(response);
}

std::vector<proto::PositionUpdate> GrvtDataFetcher::get_positions() {
    if (!is_authenticated()) {
        std::cerr << "[GRVT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("positions", "");
    if (response.empty()) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to fetch positions" << std::endl;
        return {};
    }
    
    return parse_positions(response);
}

std::string GrvtDataFetcher::make_request(const std::string& method, const std::string& params) {
    if (!curl_) return "";
    
    std::string url = config_.base_url + "/api/v1/" + method;
    
    std::string response_data;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    if (!params.empty()) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, params.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, DataFetcherWriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, config_.timeout_ms / 1000);
    
    // Add headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_headers = create_auth_headers();
    if (!auth_headers.empty()) {
        headers = curl_slist_append(headers, auth_headers.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "[GRVT_DATA_FETCHER] CURL error: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    return response_data;
}

std::string GrvtDataFetcher::create_auth_headers() {
    std::stringstream headers;
    headers << "Cookie: " << config_.session_cookie << "\r\n";
    headers << "X-Grvt-Account-Id: " << config_.account_id << "\r\n";
    return headers.str();
}

std::vector<proto::OrderEvent> GrvtDataFetcher::parse_orders(const std::string& json_response) {
    std::vector<proto::OrderEvent> orders;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to parse orders JSON" << std::endl;
        return orders;
    }
    
    const Json::Value& result = root["result"];
    if (result.isArray()) {
        for (const auto& order_data : result) {
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(order_data["orderId"].asString());
            order_event.set_exch("GRVT");
            order_event.set_symbol(order_data["symbol"].asString());
            order_event.set_exch_order_id(order_data["orderId"].asString());
            order_event.set_fill_qty(order_data["quantity"].asDouble());
            order_event.set_fill_price(order_data["price"].asDouble());
            order_event.set_timestamp_us(order_data["time"].asUInt64() * 1000); // Convert to microseconds
            
            // Map GRVT order status to OrderEventType
            std::string status = order_data["status"].asString();
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
            
            orders.push_back(order_event);
        }
    }
    
    return orders;
}

std::vector<proto::PositionUpdate> GrvtDataFetcher::parse_positions(const std::string& json_response) {
    std::vector<proto::PositionUpdate> positions;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to parse positions JSON" << std::endl;
        return positions;
    }
    
    const Json::Value& result = root["result"];
    if (result.isArray()) {
        for (const auto& pos_data : result) {
            double position_amt = pos_data["positionAmt"].asDouble();
            if (std::abs(position_amt) < 1e-8) continue; // Skip zero positions
            
            proto::PositionUpdate position;
            position.set_exch("GRVT");
            position.set_symbol(pos_data["symbol"].asString());
            position.set_qty(std::abs(position_amt));
            position.set_avg_price(pos_data["entryPrice"].asDouble());
            position.set_timestamp_us(pos_data["updateTime"].asUInt64() * 1000); // Convert to microseconds
            
            positions.push_back(position);
        }
    }
    
    return positions;
}

size_t GrvtDataFetcher::DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

}