#include "grvt_data_fetcher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <json/json.h>

namespace grvt {

GrvtDataFetcher::GrvtDataFetcher(const std::string& api_key, const std::string& session_cookie, const std::string& account_id)
    : curl_(nullptr), authenticated_(false), auth_environment_(GrvtAuthEnvironment::PRODUCTION) {
    config_.api_key = api_key;
    config_.session_cookie = session_cookie;
    config_.account_id = account_id;
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to initialize CURL" << std::endl;
    }
    authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
}

GrvtDataFetcher::GrvtDataFetcher(const std::string& api_key, GrvtAuthEnvironment env)
    : curl_(nullptr), authenticated_(false), auth_environment_(env) {
    config_.api_key = api_key;
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to initialize CURL" << std::endl;
    }
    
    // Initialize auth helper
    auth_helper_ = std::make_unique<GrvtAuth>(auth_environment_);
    
    // Authenticate with API key
    if (!api_key.empty()) {
        authenticate_with_api_key(api_key);
    }
}

GrvtDataFetcher::~GrvtDataFetcher() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void GrvtDataFetcher::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    
    // If secret is provided, use it as session cookie (backward compatibility)
    // Otherwise, authenticate with API key to get session cookie
    if (!secret.empty()) {
        config_.session_cookie = secret;
        authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
    } else if (!api_key.empty()) {
        // Authenticate with API key to get session cookie and account ID
        authenticate_with_api_key(api_key);
    } else {
        authenticated_.store(false);
    }
}

bool GrvtDataFetcher::authenticate_with_api_key(const std::string& api_key, const std::string& sub_account_id) {
    if (api_key.empty()) {
        std::cerr << "[GRVT_DATA_FETCHER] Cannot authenticate: API key is empty" << std::endl;
        return false;
    }
    
    config_.api_key = api_key;
    
    // Initialize auth helper if not already initialized
    if (!auth_helper_) {
        auth_helper_ = std::make_unique<GrvtAuth>(auth_environment_);
    }
    
    // Perform authentication
    GrvtAuthResult auth_result = auth_helper_->authenticate(api_key, sub_account_id);
    
    if (auth_result.is_valid()) {
        config_.session_cookie = auth_result.session_cookie;
        config_.account_id = auth_result.account_id;
        authenticated_.store(true);
        std::cout << "[GRVT_DATA_FETCHER] Authentication successful" << std::endl;
        std::cout << "[GRVT_DATA_FETCHER] Account ID: " << config_.account_id << std::endl;
        return true;
    } else {
        authenticated_.store(false);
        std::cerr << "[GRVT_DATA_FETCHER] Authentication failed: " << auth_result.error_message << std::endl;
        return false;
    }
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

std::vector<proto::AccountBalance> GrvtDataFetcher::get_balances() {
    if (!is_authenticated()) {
        std::cerr << "[GRVT_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    // Use sub-account summary endpoint for balances
    std::string params = R"({"sub_account_id":")" + config_.account_id + R"("})";
    std::string response = make_request("sub_account_summary", params);
    if (response.empty()) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to fetch balances" << std::endl;
        return {};
    }
    
    return parse_balances(response);
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
            
            // Extract filled quantity (executedQty or filledQty)
            double executed_qty = 0.0;
            if (order_data.isMember("executedQty")) {
                executed_qty = order_data["executedQty"].asDouble();
            } else if (order_data.isMember("filledQty")) {
                executed_qty = order_data["filledQty"].asDouble();
            }
            order_event.set_fill_qty(executed_qty);
            
            // Extract average fill price
            double avg_price = 0.0;
            if (order_data.isMember("avgPrice")) {
                avg_price = order_data["avgPrice"].asDouble();
            } else if (order_data.isMember("price")) {
                avg_price = order_data["price"].asDouble();
            }
            order_event.set_fill_price(avg_price);
            
            order_event.set_timestamp_us(order_data["time"].asUInt64() * 1000); // Convert to microseconds
            
            // Extract original order quantity and store in text field
            // Format: "origQty:<value>|side:<value>|price:<value>"
            double orig_qty = order_data.isMember("quantity") ? order_data["quantity"].asDouble() : executed_qty;
            std::string side = order_data.isMember("side") ? order_data["side"].asString() : "BUY";
            double price = order_data.isMember("price") ? order_data["price"].asDouble() : avg_price;
            std::ostringstream metadata;
            metadata << "origQty:" << orig_qty << "|side:" << side << "|price:" << price;
            order_event.set_text(metadata.str());
            
            // Map GRVT order status to OrderEventType
            // Note: For partially filled orders, status might be "PARTIALLY_FILLED" but we use FILL event type
            std::string status = order_data["status"].asString();
            if (status == "NEW") {
                order_event.set_event_type(proto::OrderEventType::ACK);
            } else if (status == "PARTIALLY_FILLED" || status == "PARTIAL") {
                // Partially filled orders are still open, use FILL event type
                order_event.set_event_type(proto::OrderEventType::FILL);
            } else if (status == "FILLED") {
                order_event.set_event_type(proto::OrderEventType::FILL);
            } else if (status == "CANCELED" || status == "CANCELLED") {
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

std::vector<proto::AccountBalance> GrvtDataFetcher::parse_balances(const std::string& json_response) {
    std::vector<proto::AccountBalance> balances;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[GRVT_DATA_FETCHER] Failed to parse balances JSON" << std::endl;
        return balances;
    }
    
    const Json::Value& result = root["result"];
    if (result.isMember("spot_balances") && result["spot_balances"].isArray()) {
        const Json::Value& spot_balances = result["spot_balances"];
        
        for (const auto& balance_data : spot_balances) {
            double balance_amount = balance_data["balance"].asDouble();
            if (balance_amount < 1e-8) continue; // Skip zero balances
            
            proto::AccountBalance balance;
            balance.set_exch("GRVT");
            balance.set_instrument(balance_data["currency"].asString());
            balance.set_balance(balance_amount);
            balance.set_available(balance_data["available"].asDouble());
            balance.set_locked(balance_data["locked"].asDouble());
            balance.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            balances.push_back(balance);
        }
    }
    
    return balances;
}

size_t GrvtDataFetcher::DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

}