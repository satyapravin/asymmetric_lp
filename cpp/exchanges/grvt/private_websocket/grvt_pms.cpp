#include "grvt_pms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace grvt {

GrvtPMS::GrvtPMS(const GrvtPMSConfig& config) : config_(config) {
    std::cout << "[GRVT_PMS] Initializing GRVT PMS" << std::endl;
}

GrvtPMS::~GrvtPMS() {
    disconnect();
}

bool GrvtPMS::connect() {
    std::cout << "[GRVT_PMS] Connecting to GRVT WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[GRVT_PMS] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&GrvtPMS::websocket_loop, this);
        
        // Start balance polling thread
        polling_running_ = true;
        polling_thread_ = std::thread(&GrvtPMS::polling_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[GRVT_PMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        std::cout << "[GRVT_PMS] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_PMS] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void GrvtPMS::disconnect() {
    std::cout << "[GRVT_PMS] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    polling_running_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    connected_ = false;
    authenticated_ = false;
    
    std::cout << "[GRVT_PMS] Disconnected" << std::endl;
}

bool GrvtPMS::is_connected() const {
    return connected_.load();
}

void GrvtPMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    config_.session_cookie = secret; // GRVT uses session cookie instead of secret
    authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
}

bool GrvtPMS::is_authenticated() const {
    return authenticated_.load();
}

void GrvtPMS::set_position_update_callback(PositionUpdateCallback callback) {
    position_update_callback_ = callback;
}

void GrvtPMS::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    account_balance_update_callback_ = callback;
    std::cout << "[GRVT_PMS] Account balance update callback set" << std::endl;
}

void GrvtPMS::websocket_loop() {
    std::cout << "[GRVT_PMS] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional position updates
            static int counter = 0;
            if (++counter % 30 == 0) {
                std::string mock_position_update = R"({"jsonrpc":"2.0","method":"position_update","params":{"symbol":"BTCUSDT","positionAmt":0.1,"entryPrice":50000,"markPrice":50100,"unrealizedPnl":10.0,"updateTime":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                handle_websocket_message(mock_position_update);
            }
            
            // Simulate account updates
            if (counter % 60 == 0) {
                std::string mock_account_update = R"({"jsonrpc":"2.0","method":"account_update","params":{"totalWalletBalance":10000.0,"totalUnrealizedPnl":10.0,"totalMarginBalance":10010.0,"updateTime":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                handle_websocket_message(mock_account_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[GRVT_PMS] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[GRVT_PMS] WebSocket loop stopped" << std::endl;
}

void GrvtPMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[GRVT_PMS] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "position_update" && root.isMember("params")) {
                handle_position_update(root["params"]);
            } else if (method == "account_update" && root.isMember("params")) {
                handle_account_update(root["params"]);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_PMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void GrvtPMS::handle_position_update(const Json::Value& position_data) {
    double position_amt = position_data["positionAmt"].asDouble();
    if (std::abs(position_amt) < 1e-8) return; // Skip zero positions
    
    proto::PositionUpdate position;
    position.set_exch("GRVT");
    position.set_symbol(position_data["symbol"].asString());
    position.set_qty(std::abs(position_amt));
    position.set_avg_price(position_data["entryPrice"].asDouble());
    // Note: mark_price and unrealized_pnl not available in proto::PositionUpdate
    // position.set_mark_price(position_data["markPrice"].asDouble());
    // position.set_unrealized_pnl(position_data["unrealizedPnl"].asDouble());
    position.set_timestamp_us(position_data["updateTime"].asUInt64() * 1000); // Convert to microseconds
    
    if (position_update_callback_) {
        position_update_callback_(position);
    }
    
    std::cout << "[GRVT_PMS] Position update: " << position.symbol() 
              << " qty: " << position.qty() 
              << " price: " << position.avg_price() 
              << " pnl: N/A" << std::endl;
}

void GrvtPMS::handle_account_update(const Json::Value& account_data) {
    std::cout << "[GRVT_PMS] Account update: " << account_data.toStyledString() << std::endl;
    
    // Note: Balance updates are now handled via REST API polling, not WebSocket
    // This method only handles position updates from WebSocket
}

void GrvtPMS::handle_balance_update(const Json::Value& balance_data) {
    proto::AccountBalanceUpdate balance_update;
    
    // GRVT REST API returns spot_balances array
    if (balance_data.isMember("spot_balances") && balance_data["spot_balances"].isArray()) {
        const Json::Value& spot_balances = balance_data["spot_balances"];
        
        for (const auto& balance : spot_balances) {
            proto::AccountBalance* acc_balance = balance_update.add_balances();
            
            acc_balance->set_exch("GRVT");
            acc_balance->set_instrument(balance["currency"].asString());
            acc_balance->set_balance(balance["balance"].asDouble());
            acc_balance->set_available(balance["available"].asDouble());
            acc_balance->set_locked(balance["locked"].asDouble());
            acc_balance->set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }
    
    if (account_balance_update_callback_) {
        account_balance_update_callback_(balance_update);
    }
    
    std::cout << "[GRVT_PMS] Balance update: " << balance_update.balances_size() << " balances" << std::endl;
}

bool GrvtPMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    std::cout << "[GRVT_PMS] Authenticating: " << auth_msg << std::endl;
    
    // Mock authentication response
    std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"authenticated":true}})";
    handle_websocket_message(mock_auth_response);
    
    return true;
}

std::string GrvtPMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "auth";
    
    Json::Value params;
    params["apiKey"] = config_.api_key;
    params["sessionCookie"] = config_.session_cookie;
    params["accountId"] = config_.account_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string GrvtPMS::generate_request_id() {
    return std::to_string(request_id_++);
}

// Balance polling methods
void GrvtPMS::polling_loop() {
    std::cout << "[GRVT_PMS] Balance polling loop started" << std::endl;
    
    while (polling_running_.load()) {
        try {
            poll_account_balances();
            
            // Sleep for the configured interval
            std::this_thread::sleep_for(std::chrono::seconds(config_.polling_interval_seconds));
            
        } catch (const std::exception& e) {
            std::cerr << "[GRVT_PMS] Polling error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds on error
        }
    }
    
    std::cout << "[GRVT_PMS] Balance polling loop stopped" << std::endl;
}

void GrvtPMS::poll_account_balances() {
    if (!connected_.load() || config_.sub_account_id.empty()) {
        return;
    }
    
    std::string request = create_balance_request();
    std::cout << "[GRVT_PMS] Polling account balances: " << request << std::endl;
    
    // Mock REST API response for now
    // In real implementation, this would make HTTP request to GRVT API
    std::string mock_response = R"({
        "result": {
            "sub_account_id": ")" + config_.sub_account_id + R"(",
            "spot_balances": [
                {
                    "currency": "USDT",
                    "balance": 1000.0,
                    "available": 950.0,
                    "locked": 50.0
                },
                {
                    "currency": "BTC",
                    "balance": 0.1,
                    "available": 0.08,
                    "locked": 0.02
                }
            ]
        }
    })";
    
    parse_balance_response(mock_response);
}

std::string GrvtPMS::create_balance_request() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "private/get_sub_account_summary";
    
    Json::Value params;
    params["sub_account_id"] = config_.sub_account_id;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

bool GrvtPMS::parse_balance_response(const std::string& response) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(response, root)) {
            std::cerr << "[GRVT_PMS] Failed to parse balance response" << std::endl;
            return false;
        }
        
        if (root.isMember("result")) {
            handle_balance_update(root["result"]);
            return true;
        } else if (root.isMember("error")) {
            std::cerr << "[GRVT_PMS] API error: " << root["error"].toStyledString() << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[GRVT_PMS] Error parsing balance response: " << e.what() << std::endl;
        return false;
    }
    
    return false;
}

} // namespace grvt
