#include "binance_data_fetcher.hpp"
#include "../../utils/handlers/http/i_http_handler.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <json/json.h>
#include <string>
#include <memory>
#include <ctime>
#include <cstdio>

namespace binance {

BinanceDataFetcher::BinanceDataFetcher() {
    base_url_ = "https://api.binance.com";
}

std::vector<BinanceOrder> BinanceDataFetcher::get_active_orders() {
    std::vector<BinanceOrder> orders;
    
    if (!connected_) {
        return orders;
    }
    
    try {
        std::string response = make_request("/fapi/v1/openOrders", "GET", "", true);
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            std::cerr << "[BINANCE_FETCHER] Failed to parse orders response" << std::endl;
            return orders;
        }
        
        for (const auto& order_json : root) {
            BinanceOrder order;
            order.cl_ord_id = order_json["clientOrderId"].asString();
            order.exchange_order_id = std::to_string(order_json["orderId"].asUInt64());
            order.symbol = order_json["symbol"].asString();
            order.side = order_json["side"].asString();
            order.qty = order_json["origQty"].asDouble();
            order.price = order_json["price"].asDouble();
            order.state = order_json["status"].asString();
            order.timestamp_us = order_json["time"].asUInt64() * 1000; // Convert to microseconds
            
            orders.push_back(order);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_FETCHER] Error fetching active orders: " << e.what() << std::endl;
    }
    
    return orders;
}

std::vector<BinanceOrder> BinanceDataFetcher::get_order_history(const std::string& symbol, 
                                                               uint64_t start_time, 
                                                               uint64_t end_time) {
    std::vector<BinanceOrder> orders;
    
    if (!connected_) {
        return orders;
    }
    
    try {
        std::stringstream endpoint;
        endpoint << "/fapi/v1/allOrders?symbol=" << symbol;
        
        if (start_time > 0) {
            endpoint << "&startTime=" << start_time;
        }
        if (end_time > 0) {
            endpoint << "&endTime=" << end_time;
        }
        
        std::string response = make_request(endpoint.str(), "GET", "", true);
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            std::cerr << "[BINANCE_FETCHER] Failed to parse order history response" << std::endl;
            return orders;
        }
        
        for (const auto& order_json : root) {
            BinanceOrder order;
            order.cl_ord_id = order_json["clientOrderId"].asString();
            order.exchange_order_id = std::to_string(order_json["orderId"].asUInt64());
            order.symbol = order_json["symbol"].asString();
            order.side = order_json["side"].asString();
            order.qty = order_json["origQty"].asDouble();
            order.price = order_json["price"].asDouble();
            order.state = order_json["status"].asString();
            order.timestamp_us = order_json["time"].asUInt64() * 1000;
            
            orders.push_back(order);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_FETCHER] Error fetching order history: " << e.what() << std::endl;
    }
    
    return orders;
}

std::vector<BinancePosition> BinanceDataFetcher::get_positions() {
    std::vector<BinancePosition> positions;
    
    if (!connected_) {
        return positions;
    }
    
    try {
        std::string response = make_request("/fapi/v2/positionRisk", "GET", "", true);
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            std::cerr << "[BINANCE_FETCHER] Failed to parse positions response" << std::endl;
            return positions;
        }
        
        for (const auto& position_json : root) {
            BinancePosition position;
            position.symbol = position_json["symbol"].asString();
            position.qty = std::stod(position_json["positionAmt"].asString());
            position.avg_price = std::stod(position_json["entryPrice"].asString());
            position.unrealized_pnl = std::stod(position_json["unRealizedProfit"].asString());
            position.margin_used = std::stod(position_json["isolatedWallet"].asString());
            position.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Only include positions with non-zero quantity
            if (std::abs(position.qty) > 1e-8) {
                positions.push_back(position);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_FETCHER] Error fetching positions: " << e.what() << std::endl;
    }
    
    return positions;
}

std::vector<BinanceTrade> BinanceDataFetcher::get_trade_history(const std::string& symbol,
                                                              uint64_t start_time,
                                                              uint64_t end_time) {
    std::vector<BinanceTrade> trades;
    
    if (!connected_) {
        return trades;
    }
    
    try {
        std::stringstream endpoint;
        endpoint << "/fapi/v1/userTrades?symbol=" << symbol;
        
        if (start_time > 0) {
            endpoint << "&startTime=" << start_time;
        }
        if (end_time > 0) {
            endpoint << "&endTime=" << end_time;
        }
        
        std::string response = make_request(endpoint.str(), "GET", "", true);
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            std::cerr << "[BINANCE_FETCHER] Failed to parse trades response" << std::endl;
            return trades;
        }
        
        for (const auto& trade_json : root) {
            BinanceTrade trade;
            trade.cl_ord_id = trade_json["clientOrderId"].asString();
            trade.exchange_order_id = std::to_string(trade_json["orderId"].asUInt64());
            trade.symbol = trade_json["symbol"].asString();
            trade.side = trade_json["side"].asString();
            trade.qty = std::stod(trade_json["qty"].asString());
            trade.price = std::stod(trade_json["price"].asString());
            trade.commission = std::stod(trade_json["commission"].asString());
            trade.timestamp_us = trade_json["time"].asUInt64() * 1000;
            
            trades.push_back(trade);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_FETCHER] Error fetching trade history: " << e.what() << std::endl;
    }
    
    return trades;
}

std::vector<BinanceBalance> BinanceDataFetcher::get_balances() {
    std::vector<BinanceBalance> balances;
    
    if (!connected_) {
        return balances;
    }
    
    try {
        std::string response = make_request("/fapi/v2/account", "GET", "", true);
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            std::cerr << "[BINANCE_FETCHER] Failed to parse balances response" << std::endl;
            return balances;
        }
        
        const auto& assets = root["assets"];
        for (const auto& asset_json : assets) {
            BinanceBalance balance;
            balance.asset = asset_json["asset"].asString();
            balance.free_balance = std::stod(asset_json["availableBalance"].asString());
            balance.locked_balance = std::stod(asset_json["initialMargin"].asString());
            balance.total_balance = balance.free_balance + balance.locked_balance;
            balance.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Only include balances with non-zero total
            if (balance.total_balance > 1e-8) {
                balances.push_back(balance);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_FETCHER] Error fetching balances: " << e.what() << std::endl;
    }
    
    return balances;
}

bool BinanceDataFetcher::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

void BinanceDataFetcher::set_api_credentials(const std::string& api_key, const std::string& api_secret) {
    std::lock_guard<std::mutex> lock(mutex_);
    api_key_ = api_key;
    api_secret_ = api_secret;
}

void BinanceDataFetcher::set_base_url(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    base_url_ = url;
}

void BinanceDataFetcher::set_testnet_mode(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    testnet_mode_ = enabled;
    if (enabled) {
        base_url_ = "https://testnet.binance.vision";
    } else {
        base_url_ = "https://api.binance.com";
    }
}

std::string BinanceDataFetcher::make_request(const std::string& endpoint, const std::string& method, 
                                            const std::string& body, bool is_signed) {
    // This would use the HTTP handler from our refactored architecture
    // For now, return empty string as placeholder
    std::cout << "[BINANCE_FETCHER] Making request to " << base_url_ << endpoint << std::endl;
    return "{}";
}

std::string BinanceDataFetcher::generate_signature(const std::string& data) {
    unsigned char* digest;
    unsigned int digest_len;
    
    digest = HMAC(EVP_sha256(), 
                  api_secret_.c_str(), api_secret_.length(),
                  (unsigned char*)data.c_str(), data.length(),
                  nullptr, &digest_len);
    
    char md_string[65];
    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(&md_string[i*2], "%02x", (unsigned int)digest[i]);
    }
    
    return std::string(md_string);
}

std::string BinanceDataFetcher::create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) {
    std::map<std::string, std::string> headers;
    
    // Add API key header
    headers["X-MBX-APIKEY"] = api_key_;
    
    // Add signature to body
    std::string query_string = body.empty() ? "" : body;
    if (!query_string.empty() && query_string.find("timestamp=") == std::string::npos) {
        query_string += "&timestamp=" + std::to_string(std::time(nullptr) * 1000);
    }
    
    std::string signature = generate_signature(query_string);
    query_string += "&signature=" + signature;
    
    return headers;
}

} // namespace binance
