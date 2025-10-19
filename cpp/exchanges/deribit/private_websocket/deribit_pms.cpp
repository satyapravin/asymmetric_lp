#include "deribit_pms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace deribit {

DeribitPMS::DeribitPMS(const DeribitPMSConfig& config) : config_(config) {
    std::cout << "[DERIBIT_PMS] Initializing Deribit PMS" << std::endl;
}

DeribitPMS::~DeribitPMS() {
    disconnect();
}

bool DeribitPMS::connect() {
    std::cout << "[DERIBIT_PMS] Connecting to Deribit WebSocket..." << std::endl;
    
    if (connected_.load()) {
        std::cout << "[DERIBIT_PMS] Already connected" << std::endl;
        return true;
    }
    
    try {
        // Initialize WebSocket connection (mock implementation)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitPMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[DERIBIT_PMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_ = true;
        
        std::cout << "[DERIBIT_PMS] Connected successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_PMS] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void DeribitPMS::disconnect() {
    std::cout << "[DERIBIT_PMS] Disconnecting..." << std::endl;
    
    websocket_running_ = false;
    connected_ = false;
    authenticated_ = false;
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
    
    std::cout << "[DERIBIT_PMS] Disconnected" << std::endl;
}

bool DeribitPMS::is_connected() const {
    return connected_.load();
}

void DeribitPMS::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    config_.client_id = api_key;
    config_.client_secret = secret;
    authenticated_.store(!config_.client_id.empty() && !config_.client_secret.empty());
}

bool DeribitPMS::is_authenticated() const {
    return authenticated_.load();
}

void DeribitPMS::set_position_update_callback(PositionUpdateCallback callback) {
    position_update_callback_ = callback;
}

void DeribitPMS::websocket_loop() {
    std::cout << "[DERIBIT_PMS] WebSocket loop started" << std::endl;
    
    while (websocket_running_) {
        try {
            // Mock WebSocket message processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Simulate occasional position updates
            static int counter = 0;
            if (++counter % 30 == 0) {
                std::string mock_position_update = R"({"jsonrpc":"2.0","method":"user.portfolio","params":{"instrument_name":"BTC-PERPETUAL","size":0.1,"average_price":50000,"mark_price":50100,"unrealized_pnl":10.0,"timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                handle_websocket_message(mock_position_update);
            }
            
            // Simulate account updates
            if (counter % 60 == 0) {
                std::string mock_account_update = R"({"jsonrpc":"2.0","method":"user.changes","params":{"total_balance":10000.0,"total_unrealized_pnl":10.0,"total_margin_balance":10010.0,"timestamp":)" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + R"(}})";
                handle_websocket_message(mock_account_update);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DERIBIT_PMS] WebSocket loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[DERIBIT_PMS] WebSocket loop stopped" << std::endl;
}

void DeribitPMS::handle_websocket_message(const std::string& message) {
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message, root)) {
            std::cerr << "[DERIBIT_PMS] Failed to parse WebSocket message" << std::endl;
            return;
        }
        
        // Handle different message types
        if (root.isMember("method")) {
            std::string method = root["method"].asString();
            
            if (method == "user.portfolio" && root.isMember("params")) {
                handle_position_update(root["params"]);
            } else if (method == "user.changes" && root.isMember("params")) {
                handle_account_update(root["params"]);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_PMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void DeribitPMS::handle_position_update(const Json::Value& position_data) {
    double position_size = position_data["size"].asDouble();
    if (std::abs(position_size) < 1e-8) return; // Skip zero positions
    
    proto::PositionUpdate position;
    position.set_exch("DERIBIT");
    position.set_symbol(position_data["instrument_name"].asString());
    position.set_qty(std::abs(position_size));
    position.set_avg_price(position_data["average_price"].asDouble());
    // Note: mark_price and unrealized_pnl not available in proto::PositionUpdate
    // position.set_mark_price(position_data["mark_price"].asDouble());
    // position.set_unrealized_pnl(position_data["unrealized_pnl"].asDouble());
    position.set_timestamp_us(position_data["timestamp"].asUInt64() * 1000); // Convert to microseconds
    
    if (position_update_callback_) {
        position_update_callback_(position);
    }
    
    std::cout << "[DERIBIT_PMS] Position update: " << position.symbol() 
              << " qty: " << position.qty() 
              << " price: " << position.avg_price() 
              << " pnl: N/A" << std::endl;
}

void DeribitPMS::handle_account_update(const Json::Value& account_data) {
    std::cout << "[DERIBIT_PMS] Account update: " << account_data.toStyledString() << std::endl;
    
    // Create a position update for account-level information
    proto::PositionUpdate account_position;
    account_position.set_exch("DERIBIT");
    account_position.set_symbol("ACCOUNT");
    account_position.set_qty(0.0);
    account_position.set_avg_price(0.0);
    // Note: mark_price and unrealized_pnl not available in proto::PositionUpdate
    // account_position.set_mark_price(0.0);
    // account_position.set_unrealized_pnl(account_data["total_unrealized_pnl"].asDouble());
    account_position.set_timestamp_us(account_data["timestamp"].asUInt64() * 1000);
    
    if (position_update_callback_) {
        position_update_callback_(account_position);
    }
}

bool DeribitPMS::authenticate_websocket() {
    std::string auth_msg = create_auth_message();
    std::cout << "[DERIBIT_PMS] Authenticating: " << auth_msg << std::endl;
    
    // Mock authentication response
    std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + R"(,"result":{"access_token":"mock_token","expires_in":3600}})";
    handle_websocket_message(mock_auth_response);
    
    return true;
}

std::string DeribitPMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = generate_request_id();
    root["method"] = "public/auth";
    
    Json::Value params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string DeribitPMS::generate_request_id() {
    return std::to_string(request_id_++);
}

} // namespace deribit
