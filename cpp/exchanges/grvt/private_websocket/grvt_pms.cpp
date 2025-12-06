#include "grvt_pms.hpp"
#include "../grvt_auth.hpp"
#include "../../../utils/logging/log_helper.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace grvt {

GrvtPMS::GrvtPMS(const GrvtPMSConfig& config) : config_(config) {
    LOG_INFO_COMP("GRVT_PMS", "Initializing GRVT PMS");
}

GrvtPMS::~GrvtPMS() {
    disconnect();
}

bool GrvtPMS::connect() {
    LOG_INFO_COMP("GRVT_PMS", "Connecting to GRVT WebSocket...");
    
    if (connected_.load()) {
        LOG_INFO_COMP("GRVT_PMS", "Already connected");
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
            LOG_ERROR_COMP("GRVT_PMS", "Authentication failed");
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        LOG_INFO_COMP("GRVT_PMS", "Connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("GRVT_PMS", "Connection failed: " + std::string(e.what()));
        return false;
    }
}

void GrvtPMS::disconnect() {
    LOG_INFO_COMP("GRVT_PMS", "Disconnecting...");
    
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
    
    LOG_INFO_COMP("GRVT_PMS", "Disconnected");
}

bool GrvtPMS::is_connected() const {
    return connected_.load();
}

void GrvtPMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    
    // If secret is provided, use it as session cookie (backward compatibility)
    // Otherwise, authenticate with API key to get session cookie
    if (!secret.empty()) {
        config_.session_cookie = secret;
        authenticated_.store(!config_.api_key.empty() && !config_.session_cookie.empty() && !config_.account_id.empty());
    } else if (!api_key.empty()) {
        // Authenticate with API key to get session cookie and account ID
        GrvtAuth auth_helper(GrvtAuthEnvironment::PRODUCTION);
        GrvtAuthResult auth_result = auth_helper.authenticate(api_key);
        
        if (auth_result.is_valid()) {
            config_.session_cookie = auth_result.session_cookie;
            config_.account_id = auth_result.account_id;
            authenticated_.store(true);
            LOG_INFO_COMP("GRVT_PMS", "Authentication successful via API key");
        } else {
            authenticated_.store(false);
            LOG_ERROR_COMP("GRVT_PMS", "Authentication failed: " + auth_result.error_message);
        }
    } else {
        authenticated_.store(false);
    }
}

bool GrvtPMS::is_authenticated() const {
    return authenticated_.load();
}

void GrvtPMS::set_position_update_callback(PositionUpdateCallback callback) {
    position_update_callback_ = callback;
}

void GrvtPMS::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    account_balance_update_callback_ = callback;
    LOG_INFO_COMP("GRVT_PMS", "Account balance update callback set");
}

void GrvtPMS::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    // GRVT uses REST API polling, not WebSocket for balance updates
    LOG_INFO_COMP("GRVT_PMS", "WebSocket transport injection not supported (uses REST API polling)");
}

void GrvtPMS::websocket_loop() {
    LOG_INFO_COMP("GRVT_PMS", "WebSocket loop started");
    
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
            LOG_ERROR_COMP("GRVT_PMS", "WebSocket loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    LOG_INFO_COMP("GRVT_PMS", "WebSocket loop stopped");
}

void GrvtPMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            LOG_ERROR_COMP("GRVT_PMS", "Failed to parse WebSocket message");
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
        LOG_ERROR_COMP("GRVT_PMS", "Error handling WebSocket message: " + std::string(e.what()));
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
    
    std::string log_msg = "Position update: " + position.symbol() + 
                          " qty: " + std::to_string(position.qty()) + 
                          " price: " + std::to_string(position.avg_price()) + 
                          " pnl: N/A";
    LOG_DEBUG_COMP("GRVT_PMS", log_msg);
}

void GrvtPMS::handle_account_update(const Json::Value& account_data) {
    LOG_DEBUG_COMP("GRVT_PMS", "Account update: " + account_data.toStyledString());
    
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
    
    LOG_DEBUG_COMP("GRVT_PMS", "Balance update: " + std::to_string(balance_update.balances_size()) + " balances");
}

bool GrvtPMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    LOG_INFO_COMP("GRVT_PMS", "Authenticating: " + auth_msg);
    
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
    LOG_INFO_COMP("GRVT_PMS", "Balance polling loop started");
    
    while (polling_running_.load()) {
        try {
            poll_account_balances();
            
            // Sleep for the configured interval
            std::this_thread::sleep_for(std::chrono::seconds(config_.polling_interval_seconds));
            
        } catch (const std::exception& e) {
            LOG_ERROR_COMP("GRVT_PMS", "Polling error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds on error
        }
    }
    
    LOG_INFO_COMP("GRVT_PMS", "Balance polling loop stopped");
}

void GrvtPMS::poll_account_balances() {
    if (!connected_.load() || config_.sub_account_id.empty()) {
        return;
    }
    
    std::string request = create_balance_request();
    LOG_DEBUG_COMP("GRVT_PMS", "Polling account balances: " + request);
    
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
            LOG_ERROR_COMP("GRVT_PMS", "Failed to parse balance response");
            return false;
        }
        
        if (root.isMember("result")) {
            handle_balance_update(root["result"]);
            return true;
        } else if (root.isMember("error")) {
            LOG_ERROR_COMP("GRVT_PMS", "API error: " + root["error"].toStyledString());
            return false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_COMP("GRVT_PMS", "Error parsing balance response: " + std::string(e.what()));
        return false;
    }
    
    return false;
}

} // namespace grvt
