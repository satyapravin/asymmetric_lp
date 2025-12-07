#include "binance_data_fetcher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <json/json.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <string>

namespace binance {

BinanceDataFetcher::BinanceDataFetcher(const std::string& api_key, const std::string& api_secret)
    : api_key_(api_key), api_secret_(api_secret), curl_(nullptr), authenticated_(false) {
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[BINANCE_DATA_FETCHER] Failed to initialize CURL" << std::endl;
    }
}

BinanceDataFetcher::~BinanceDataFetcher() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void BinanceDataFetcher::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    api_key_ = api_key;
    api_secret_ = secret;
    authenticated_.store(!api_key_.empty() && !api_secret_.empty());
}

bool BinanceDataFetcher::is_authenticated() const {
    return authenticated_.load();
}

std::vector<proto::OrderEvent> BinanceDataFetcher::get_open_orders() {
    if (!is_authenticated()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string endpoint = "/fapi/v2/openOrders";
    std::string response = make_request(endpoint);
    
    if (response.empty()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Empty response for open orders" << std::endl;
        return {};
    }
    
    return parse_orders(response);
}

std::vector<proto::PositionUpdate> BinanceDataFetcher::get_positions() {
    if (!is_authenticated()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string endpoint = "/fapi/v2/positionRisk";
    std::string response = make_request(endpoint);
    
    if (response.empty()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Empty response for positions" << std::endl;
        return {};
    }
    
    return parse_positions(response);
}

std::vector<proto::AccountBalance> BinanceDataFetcher::get_balances() {
    if (!is_authenticated()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Not authenticated" << std::endl;
        return {};
    }
    
    std::string endpoint = "/fapi/v2/balance";
    std::string response = make_request(endpoint);
    
    if (response.empty()) {
        std::cerr << "[BINANCE_DATA_FETCHER] Empty response for balances" << std::endl;
        return {};
    }
    
    return parse_balances(response);
}

std::string BinanceDataFetcher::make_request(const std::string& endpoint, const std::string& params) {
    if (!curl_) {
        std::cerr << "[BINANCE_DATA_FETCHER] CURL not initialized" << std::endl;
        return "";
    }
    
    std::string url = base_url_ + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }
    
    // Add timestamp and signature for authenticated requests
    if (is_authenticated()) {
        std::string timestamp = get_timestamp();
        std::string query_string = params.empty() ? "" : params + "&";
        query_string += "timestamp=" + timestamp;
        
        std::string signature = create_signature(query_string);
        query_string += "&signature=" + signature;
        
        url = base_url_ + endpoint + "?" + query_string;
    }
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, DataFetcherWriteCallback);
    
    std::string response_data;
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);
    
    if (is_authenticated()) {
        struct curl_slist* headers = nullptr;
        std::string api_key_header = "X-MBX-APIKEY: " + api_key_;
        headers = curl_slist_append(headers, api_key_header.c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    }
    
    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        std::cerr << "[BINANCE_DATA_FETCHER] CURL error: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    long response_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code != 200) {
        std::cerr << "[BINANCE_DATA_FETCHER] HTTP error: " << response_code << std::endl;
        return "";
    }
    
    return response_data;
}

std::string BinanceDataFetcher::create_signature(const std::string& query_string) {
    unsigned char* digest = HMAC(EVP_sha256(), 
                                 api_secret_.c_str(), api_secret_.length(),
                                 (unsigned char*)query_string.c_str(), query_string.length(),
                                 nullptr, nullptr);
    
    char md_string[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&md_string[i*2], "%02x", (unsigned int)digest[i]);
    }
    
    return std::string(md_string);
}

std::string BinanceDataFetcher::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

std::vector<proto::OrderEvent> BinanceDataFetcher::parse_orders(const std::string& json_response) {
    std::vector<proto::OrderEvent> orders;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[BINANCE_DATA_FETCHER] Failed to parse JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return orders;
    }
    
    if (root.isArray()) {
        for (const auto& order_json : root) {
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(order_json["clientOrderId"].asString());
            order_event.set_exch("BINANCE");
            order_event.set_symbol(order_json["symbol"].asString());
            order_event.set_exch_order_id(order_json["orderId"].asString());
            
            // Extract filled quantity (executedQty)
            double executed_qty = 0.0;
            if (order_json.isMember("executedQty") && !order_json["executedQty"].asString().empty()) {
                executed_qty = std::stod(order_json["executedQty"].asString());
            }
            order_event.set_fill_qty(executed_qty);
            
            // Extract average fill price
            double avg_price = 0.0;
            if (order_json.isMember("avgPrice") && !order_json["avgPrice"].asString().empty()) {
                avg_price = std::stod(order_json["avgPrice"].asString());
            }
            order_event.set_fill_price(avg_price);
            
            order_event.set_timestamp_us(order_json["time"].asUInt64() * 1000); // Convert to microseconds
            
            // Extract original order quantity and store in text field (since OrderEvent doesn't have origQty field)
            // Format: "origQty:<value>|side:<value>|price:<value>"
            double orig_qty = 0.0;
            if (order_json.isMember("origQty") && !order_json["origQty"].asString().empty()) {
                orig_qty = std::stod(order_json["origQty"].asString());
            }
            std::string side = order_json.isMember("side") ? order_json["side"].asString() : "BUY";
            double price = 0.0;
            if (order_json.isMember("price") && !order_json["price"].asString().empty()) {
                price = std::stod(order_json["price"].asString());
            }
            std::ostringstream metadata;
            metadata << "origQty:" << orig_qty << "|side:" << side << "|price:" << price;
            order_event.set_text(metadata.str());
            
            // Map Binance order status to OrderEventType
            // Note: For partially filled orders, status is "PARTIALLY_FILLED" but we use FILL event type
            std::string status = order_json["status"].asString();
            if (status == "NEW") {
                order_event.set_event_type(proto::OrderEventType::ACK);
            } else if (status == "PARTIALLY_FILLED") {
                // Partially filled orders are still open, use FILL event type
                order_event.set_event_type(proto::OrderEventType::FILL);
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

std::vector<proto::PositionUpdate> BinanceDataFetcher::parse_positions(const std::string& json_response) {
    std::vector<proto::PositionUpdate> positions;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[BINANCE_DATA_FETCHER] Failed to parse JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return positions;
    }
    
    if (root.isArray()) {
        for (const auto& pos_json : root) {
            double position_amt = std::stod(pos_json["positionAmt"].asString());
            if (std::abs(position_amt) < 1e-8) continue; // Skip zero positions
            
            proto::PositionUpdate position;
            position.set_exch("BINANCE");
            position.set_symbol(pos_json["symbol"].asString());
            position.set_qty(std::abs(position_amt));
            position.set_avg_price(std::stod(pos_json["entryPrice"].asString()));
            position.set_timestamp_us(pos_json["updateTime"].asUInt64() * 1000); // Convert to microseconds
            
            positions.push_back(position);
        }
    }
    
    return positions;
}

std::vector<proto::AccountBalance> BinanceDataFetcher::parse_balances(const std::string& json_response) {
    std::vector<proto::AccountBalance> balances;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        std::cerr << "[BINANCE_DATA_FETCHER] Failed to parse JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return balances;
    }
    
    if (root.isArray()) {
        for (const auto& balance_json : root) {
            double balance_amount = std::stod(balance_json["balance"].asString());
            if (balance_amount < 1e-8) continue; // Skip zero balances
            
            proto::AccountBalance balance;
            balance.set_exch("BINANCE");
            balance.set_instrument(balance_json["asset"].asString());
            balance.set_balance(balance_amount);
            balance.set_available(std::stod(balance_json["availableBalance"].asString()));
            balance.set_locked(balance_amount - balance.available());
            balance.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            balances.push_back(balance);
        }
    }
    
    return balances;
}

size_t BinanceDataFetcher::DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

}

