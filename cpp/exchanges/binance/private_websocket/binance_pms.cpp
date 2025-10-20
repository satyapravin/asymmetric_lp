#include "binance_pms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace binance {

BinancePMS::BinancePMS(const BinancePMSConfig& config) : config_(config) {
    std::cout << "[BINANCE_PMS] Initializing Binance PMS" << std::endl;
}

BinancePMS::~BinancePMS() {
    disconnect();
}

bool BinancePMS::connect() {
    std::cout << "[BINANCE_PMS] Connecting to Binance WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[BINANCE_PMS] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&BinancePMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[BINANCE_PMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        std::cout << "[BINANCE_PMS] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_PMS] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void BinancePMS::disconnect() {
    std::cout << "[BINANCE_PMS] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    connected_ = false;
    authenticated_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    std::cout << "[BINANCE_PMS] Disconnected" << std::endl;
}

bool BinancePMS::is_connected() const {
    return connected_.load();
}

void BinancePMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.api_key = api_key;
    config_.api_secret = secret;
    authenticated_.store(!config_.api_key.empty() && !config_.api_secret.empty());
}

bool BinancePMS::is_authenticated() const {
    return authenticated_.load();
}

void BinancePMS::websocket_loop() {
    std::cout << "[BINANCE_PMS] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional position updates
            static int counter = 0;
            if (++counter % 30 == 0) {
                std::string mock_position_update = R"({"e":"ACCOUNT_UPDATE","E":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"T":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"a":{"B":[{"a":"USDT","wb":"10000.00000000","cw":"10000.00000000"}],"P":[{"s":"BTCUSDT","pa":"0.1","ep":"50000.00","cr":"0.00","up":"10.00","mt":"isolated","iw":"0.00","ps":"LONG"}],"m":"UPDATE"}})";
                handle_websocket_message(mock_position_update);
            }
            
            // Simulate account updates
            if (counter % 60 == 0) {
                std::string mock_account_update = R"({"e":"ACCOUNT_UPDATE","E":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"T":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(,"a":{"B":[{"a":"USDT","wb":"10000.00000000","cw":"10000.00000000"}],"P":[],"m":"UPDATE"}})";
                handle_websocket_message(mock_account_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[BINANCE_PMS] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[BINANCE_PMS] WebSocket loop stopped" << std::endl;
}

void BinancePMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[BINANCE_PMS] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("e")) {
            std::string event_type = root["e"].asString();
            
            if (event_type == "ACCOUNT_UPDATE" && root.isMember("a")) {
                Json::Value account_data = root["a"];
                
                // Handle position updates
                if (account_data.isMember("P")) {
                    const Json::Value& positions = account_data["P"];
                    if (positions.isArray()) {
                        for (const auto& pos_data : positions) {
                            handle_position_update(pos_data);
                        }
                    }
                }
                
                // Handle account updates
                handle_account_update(account_data);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[BINANCE_PMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void BinancePMS::handle_position_update(const Json::Value& position_data) {
    double position_amt = std::stod(position_data["pa"].asString());
    if (std::abs(position_amt) < 1e-8) return; // Skip zero positions
    
    proto::PositionUpdate position;
    position.set_exch("BINANCE");
    position.set_symbol(position_data["s"].asString());
    position.set_qty(std::abs(position_amt));
    position.set_avg_price(std::stod(position_data["ep"].asString()));
    // Note: unrealized_pnl not available in proto::PositionUpdate
    // position.set_unrealized_pnl(std::stod(position_data["up"].asString()));
    position.set_timestamp_us(position_data["E"].asUInt64() * 1000); // Convert to microseconds
    
    if (position_update_callback_) {
        position_update_callback_(position);
    }
    
    std::cout << "[BINANCE_PMS] Position update: " << position.symbol() 
              << " qty: " << position.qty() 
              << " price: " << position.avg_price() 
              << " pnl: N/A" << std::endl;
}

void BinancePMS::handle_account_update(const Json::Value& account_data) {
    std::cout << "[BINANCE_PMS] Account update: " << account_data.toStyledString() << std::endl;
    
    // Handle balance updates
    if (account_data.isMember("B")) {
        const Json::Value& balances = account_data["B"];
        if (balances.isArray() && !balances.empty()) {
            handle_balance_update(balances);
        }
    }
}

void BinancePMS::handle_balance_update(const Json::Value& balance_data) {
    proto::AccountBalanceUpdate balance_update;
    
    if (balance_data.isArray()) {
        for (const auto& balance : balance_data) {
            proto::AccountBalance* acc_balance = balance_update.add_balances();
            
            acc_balance->set_exch("BINANCE");
            acc_balance->set_instrument(balance["a"].asString()); // Asset
            acc_balance->set_balance(std::stod(balance["wb"].asString())); // Wallet balance
            acc_balance->set_available(std::stod(balance["cw"].asString())); // Cross wallet balance (available)
            acc_balance->set_locked(std::stod(balance["wb"].asString()) - std::stod(balance["cw"].asString())); // Calculated locked
            acc_balance->set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }
    
    if (account_balance_update_callback_) {
        account_balance_update_callback_(balance_update);
    }
    
    std::cout << "[BINANCE_PMS] Balance update: " << balance_update.balances_size() << " balances" << std::endl;
}

bool BinancePMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    std::cout << "[BINANCE_PMS] Authenticating: " << auth_msg << std::endl;
    
    // Mock authentication response
    std::string mock_auth_response = R"({"result":null,"id":)" + std::to_string(request_id_++) + R"(})";
    handle_websocket_message(mock_auth_response);
    
    return true;
}

std::string BinancePMS::create_auth_message() {
    Json::Value root;
    root["method"] = "SUBSCRIBE";
    root["id"] = generate_request_id();
    
    Json::Value params(Json::arrayValue);
    params.append(config_.api_key + "@account");
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string BinancePMS::generate_request_id() {
    return std::to_string(request_id_++);
}

void BinancePMS::set_position_update_callback(PositionUpdateCallback callback) {
    position_update_callback_ = callback;
    std::cout << "[BINANCE_PMS] Position update callback set" << std::endl;
}

void BinancePMS::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    account_balance_update_callback_ = callback;
    std::cout << "[BINANCE_PMS] Account balance update callback set" << std::endl;
}

} // namespace binance
