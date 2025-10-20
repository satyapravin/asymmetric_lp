#include "deribit_data_fetcher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <json/json.h>

namespace deribit {

DeribitDataFetcher::DeribitDataFetcher(const std::string& client_id, const std::string& client_secret)
    : curl_(nullptr), authenticated_(false) {
    config_.client_id = client_id;
    config_.client_secret = client_secret;
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to initialize CURL" << std::endl;
    }
}

DeribitDataFetcher::~DeribitDataFetcher() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void DeribitDataFetcher::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.client_id = api_key;
    config_.client_secret = secret;
    authenticated_.store(!config_.client_id.empty() && !config_.client_secret.empty());
}

bool DeribitDataFetcher::is_authenticated() const {
    return authenticated_.load();
}

std::vector<proto::OrderEvent> DeribitDataFetcher::get_open_orders() {
    if (!is_authenticated()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("private/get_open_orders_by_currency", R"({"currency":"BTC","kind":"future"})");
    if (response.empty()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to fetch open orders" << std::endl;
        return {};
    }
    
    return parse_orders(response);
}

std::vector<proto::PositionUpdate> DeribitDataFetcher::get_positions() {
    if (!is_authenticated()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("private/get_positions", R"({"currency":"BTC","kind":"future"})");
    if (response.empty()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to fetch positions" << std::endl;
        return {};
    }
    
    return parse_positions(response);
}

std::vector<proto::AccountBalance> DeribitDataFetcher::get_balances() {
    if (!is_authenticated()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string response = make_request("private/get_account_summary", R"({"currency":"BTC"})");
    if (response.empty()) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to fetch balances" << std::endl;
        return {};
    }
    
    return parse_balances(response);
}

std::string DeribitDataFetcher::make_request(const std::string& method, const std::string& params) {
    if (!curl_) return "";
    
    std::string url = config_.base_url + "/api/v2/" + method;
    
    std::string response_data;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, DataFetcherWriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, config_.timeout_ms / 1000);
    
    // Add headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!config_.access_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + config_.access_token;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "[DERIBIT_DATA_FETCHER] CURL error: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    return response_data;
}

std::string DeribitDataFetcher::authenticate() {
    // Mock authentication for testing
    config_.access_token = "mock_access_token_" + std::to_string(std::time(nullptr));
    return config_.access_token;
}

std::string DeribitDataFetcher::refresh_token() {
    // Mock token refresh for testing
    config_.access_token = "refreshed_token_" + std::to_string(std::time(nullptr));
    return config_.access_token;
}

std::vector<proto::OrderEvent> DeribitDataFetcher::parse_orders(const std::string& json_response) {
    std::vector<proto::OrderEvent> orders;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to parse orders JSON" << std::endl;
        return orders;
    }
    
    const Json::Value& result = root["result"];
    if (result.isArray()) {
        for (const auto& order_data : result) {
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(order_data["order_id"].asString());
            order_event.set_exch("DERIBIT");
            order_event.set_symbol(order_data["instrument_name"].asString());
            order_event.set_exch_order_id(order_data["order_id"].asString());
            order_event.set_fill_qty(order_data["amount"].asDouble());
            order_event.set_fill_price(order_data["price"].asDouble());
            order_event.set_timestamp_us(order_data["last_update_timestamp"].asUInt64() * 1000); // Convert to microseconds
            
            // Map Deribit order status to OrderEventType
            std::string status = order_data["order_state"].asString();
            if (status == "open") {
                order_event.set_event_type(proto::OrderEventType::ACK);
            } else if (status == "filled") {
                order_event.set_event_type(proto::OrderEventType::FILL);
            } else if (status == "cancelled") {
                order_event.set_event_type(proto::OrderEventType::CANCEL);
            } else if (status == "rejected") {
                order_event.set_event_type(proto::OrderEventType::REJECT);
            } else {
                order_event.set_event_type(proto::OrderEventType::ACK);  // Default to ACK
            }
            
            orders.push_back(order_event);
        }
    }
    
    return orders;
}

std::vector<proto::PositionUpdate> DeribitDataFetcher::parse_positions(const std::string& json_response) {
    std::vector<proto::PositionUpdate> positions;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to parse positions JSON" << std::endl;
        return positions;
    }
    
    const Json::Value& result = root["result"];
    if (result.isArray()) {
        for (const auto& pos_data : result) {
            double position_size = pos_data["size"].asDouble();
            if (std::abs(position_size) < 1e-8) continue; // Skip zero positions
            
            proto::PositionUpdate position;
            position.set_exch("DERIBIT");
            position.set_symbol(pos_data["instrument_name"].asString());
            position.set_qty(std::abs(position_size));
            position.set_avg_price(pos_data["average_price"].asDouble());
            // Note: mark_price and unrealized_pnl not available in proto::PositionUpdate
            // position.set_mark_price(pos_data["mark_price"].asDouble());
            // position.set_unrealized_pnl(pos_data["unrealized_pnl"].asDouble());
            position.set_timestamp_us(pos_data["timestamp"].asUInt64() * 1000); // Convert to microseconds
            
            positions.push_back(position);
        }
    }
    
    return positions;
}

std::vector<proto::AccountBalance> DeribitDataFetcher::parse_balances(const std::string& json_response) {
    std::vector<proto::AccountBalance> balances;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[DERIBIT_DATA_FETCHER] Failed to parse balances JSON" << std::endl;
        return balances;
    }
    
    const Json::Value& result = root["result"];
    if (result.isObject()) {
        // Deribit account summary contains balance information
        std::string currency = result["currency"].asString();
        double balance = result["balance"].asDouble();
        
        if (balance > 1e-8) { // Only include non-zero balances
            proto::AccountBalance account_balance;
            account_balance.set_exch("DERIBIT");
            account_balance.set_instrument(currency);
            account_balance.set_balance(balance);
            account_balance.set_available(result["available_funds"].asDouble());
            account_balance.set_locked(balance - account_balance.available());
            account_balance.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            balances.push_back(account_balance);
        }
    }
    
    return balances;
}

size_t DeribitDataFetcher::DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

}