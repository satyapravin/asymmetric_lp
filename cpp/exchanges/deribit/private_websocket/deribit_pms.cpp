#include "deribit_pms.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <json/json.h>

namespace deribit {

DeribitPMS::DeribitPMS(const DeribitPMSConfig& config) : config_(config) {
    std::cout << "[DERIBIT_PMS] Initializing Deribit PMS" << std::endl;
    
    // If credentials are provided in config, mark as authenticated
    if (!config_.client_id.empty() && !config_.client_secret.empty()) {
        authenticated_.store(true);
        std::cout << "[DERIBIT_PMS] Credentials provided in config, marked as authenticated" << std::endl;
    } else {
        authenticated_.store(false);
    }
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
        // If custom transport is set, use it (for testing)
        if (custom_transport_) {
            std::cout << "[DERIBIT_PMS] Using custom WebSocket transport" << std::endl;
            
            // Set up message callback BEFORE connecting
            custom_transport_->set_message_callback([this](const websocket_transport::WebSocketMessage& ws_msg) {
                if (!ws_msg.is_binary) {
                    handle_websocket_message(ws_msg.data);
                }
            });
            
            if (custom_transport_->connect(config_.websocket_url)) {
                connected_ = true;
                websocket_running_ = true;
                
                // Start event loop if not already running
                if (!custom_transport_->is_event_loop_running()) {
                    custom_transport_->start_event_loop();
                }
                
                websocket_thread_ = std::thread(&DeribitPMS::websocket_loop, this);
                
                // Authenticate
                if (!authenticate_websocket()) {
                    std::cerr << "[DERIBIT_PMS] Authentication failed" << std::endl;
                    return false;
                }
                
                authenticated_.store(true);
                std::cout << "[DERIBIT_PMS] Connected successfully using injected transport" << std::endl;
                return true;
            } else {
                std::cerr << "[DERIBIT_PMS] Failed to connect using custom transport" << std::endl;
                return false;
            }
        }
        
        // Initialize WebSocket connection (mock implementation for now)
        websocket_running_ = true;
        websocket_thread_ = std::thread(&DeribitPMS::websocket_loop, this);
        
        // Authenticate
        if (!authenticate_websocket()) {
            std::cerr << "[DERIBIT_PMS] Authentication failed" << std::endl;
            return false;
        }
        
        connected_ = true;
        authenticated_.store(true);
        
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
    authenticated_.store(false);
    
    if (custom_transport_) {
        custom_transport_->stop_event_loop();
    }
    
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

void DeribitPMS::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    account_balance_update_callback_ = callback;
    std::cout << "[DERIBIT_PMS] Account balance update callback set" << std::endl;
}

void DeribitPMS::set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) {
    std::cout << "[DERIBIT_PMS] Setting custom WebSocket transport for testing" << std::endl;
    custom_transport_ = transport;
}

void DeribitPMS::websocket_loop() {
    std::cout << "[DERIBIT_PMS] WebSocket loop started" << std::endl;
    
    if (custom_transport_) {
        std::cout << "[DERIBIT_PMS] Using custom transport - messages will arrive via callback" << std::endl;
        // The custom transport's event loop will handle message reception and callbacks
        // We just need to keep this thread alive while the custom transport is running
        while (websocket_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        // Mock WebSocket message processing (for testing without real connection)
        while (websocket_running_.load()) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Simulate occasional position updates (only for mock mode)
                static int counter = 0;
                if (++counter % 30 == 0) {
                    std::string mock_position_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"user.portfolio.BTC","data":{"instrument_name":"BTC-PERPETUAL","size":0.1,"average_price":50000,"mark_price":50100,"unrealized_pnl":10.0,"timestamp":)" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(}}})";
                    handle_websocket_message(mock_position_update);
                }
                
                // Simulate account updates
                if (counter % 60 == 0) {
                    std::string mock_account_update = R"({"jsonrpc":"2.0","method":"subscription","params":{"channel":"user.changes.any.any","data":{"total_balance":10000.0,"total_unrealized_pnl":10.0,"total_margin_balance":10010.0,"timestamp":)" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(}}})";
                    handle_websocket_message(mock_account_update);
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[DERIBIT_PMS] WebSocket loop error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    if (custom_transport_) {
        std::cout << "[DERIBIT_PMS] Stopping custom transport event loop" << std::endl;
        custom_transport_->stop_event_loop();
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
            
            if (method == "subscription" && root.isMember("params")) {
                Json::Value params = root["params"];
                std::string channel = params["channel"].asString();
                
                if (channel.find("user.portfolio") == 0 && params.isMember("data")) {
                    // Portfolio channel provides position updates
                    handle_position_update(params["data"]);
                } else if (channel.find("user.changes") == 0 && params.isMember("data")) {
                    // Account changes channel
                    handle_account_update(params["data"]);
                }
            }
        } else if (root.isMember("result")) {
            // Handle subscription responses
            Json::Value result = root["result"];
            if (result.isArray() && result.size() > 0) {
                std::cout << "[DERIBIT_PMS] Subscription response: " << message << std::endl;
            } else if (result.isMember("access_token")) {
                // Authentication response
                std::cout << "[DERIBIT_PMS] Authentication successful" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DERIBIT_PMS] Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void DeribitPMS::handle_position_update(const Json::Value& position_data) {
    // Deribit portfolio can contain multiple positions
    // Check if it's a single position object or contains positions array
    Json::Value positions_to_process;
    
    if (position_data.isMember("positions") && position_data["positions"].isArray()) {
        positions_to_process = position_data["positions"];
    } else if (position_data.isMember("instrument_name")) {
        // Single position object
        positions_to_process.append(position_data);
    } else {
        // Try to process as array
        if (position_data.isArray()) {
            positions_to_process = position_data;
        } else {
            return; // Unknown format
        }
    }
    
    for (const auto& pos_data : positions_to_process) {
        double position_size = 0.0;
        if (pos_data.isMember("size")) {
            if (pos_data["size"].isString()) {
                position_size = std::stod(pos_data["size"].asString());
            } else {
                position_size = pos_data["size"].asDouble();
            }
        }
        
        if (std::abs(position_size) < 1e-8) continue; // Skip zero positions
        
        proto::PositionUpdate position;
        position.set_exch("DERIBIT");
        
        if (pos_data.isMember("instrument_name")) {
            position.set_symbol(pos_data["instrument_name"].asString());
        }
        
        position.set_qty(std::abs(position_size));
        
        if (pos_data.isMember("average_price")) {
            if (pos_data["average_price"].isString()) {
                position.set_avg_price(std::stod(pos_data["average_price"].asString()));
            } else {
                position.set_avg_price(pos_data["average_price"].asDouble());
            }
        }
        
        if (pos_data.isMember("timestamp")) {
            position.set_timestamp_us(pos_data["timestamp"].asUInt64() * 1000); // Convert milliseconds to microseconds
        } else {
            position.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        
        if (position_update_callback_) {
            position_update_callback_(position);
        }
        
        std::cout << "[DERIBIT_PMS] Position update: " << position.symbol() 
                  << " qty: " << position.qty() 
                  << " price: " << position.avg_price() << std::endl;
    }
}

void DeribitPMS::handle_account_update(const Json::Value& account_data) {
    // Deribit user.changes provides account-level updates
    // This can include balance information
    if (account_data.isMember("portfolio")) {
        const Json::Value& portfolio = account_data["portfolio"];
        handle_balance_update(portfolio);
    }
    
    // Also check for positions in account update
    if (account_data.isMember("positions")) {
        const Json::Value& positions = account_data["positions"];
        if (positions.isArray()) {
            for (const auto& pos_data : positions) {
                handle_position_update(pos_data);
            }
        }
    }
}

void DeribitPMS::handle_balance_update(const Json::Value& portfolio_data) {
    proto::AccountBalanceUpdate balance_update;
    
    // Deribit portfolio structure: can be object with currency keys or array
    if (portfolio_data.isObject()) {
        // Portfolio as object with currency keys (e.g., {"BTC": {...}, "ETH": {...}})
        for (const auto& currency : portfolio_data.getMemberNames()) {
            const Json::Value& currency_data = portfolio_data[currency];
            
            proto::AccountBalance* acc_balance = balance_update.add_balances();
            acc_balance->set_exch("DERIBIT");
            acc_balance->set_instrument(currency);
            
            if (currency_data.isMember("balance")) {
                if (currency_data["balance"].isString()) {
                    acc_balance->set_balance(std::stod(currency_data["balance"].asString()));
                } else {
                    acc_balance->set_balance(currency_data["balance"].asDouble());
                }
            } else {
                acc_balance->set_balance(0.0);
            }
            
            if (currency_data.isMember("available")) {
                if (currency_data["available"].isString()) {
                    acc_balance->set_available(std::stod(currency_data["available"].asString()));
                } else {
                    acc_balance->set_available(currency_data["available"].asDouble());
                }
            } else {
                acc_balance->set_available(0.0);
            }
            
            // Calculate locked as balance - available if not provided
            acc_balance->set_locked(acc_balance->balance() - acc_balance->available());
            
            acc_balance->set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }
    
    if (account_balance_update_callback_ && balance_update.balances_size() > 0) {
        account_balance_update_callback_(balance_update);
    }
    
    if (balance_update.balances_size() > 0) {
        std::cout << "[DERIBIT_PMS] Balance update: " << balance_update.balances_size() << " balances" << std::endl;
    }
}

bool DeribitPMS::authenticate_websocket() {
    if (config_.client_id.empty() || config_.client_secret.empty()) {
        std::cerr << "[DERIBIT_PMS] Cannot authenticate: credentials not set" << std::endl;
        return false;
    }
    
    std::string auth_msg = create_auth_message();
    std::cout << "[DERIBIT_PMS] Authenticating: " << auth_msg << std::endl;
    
    // Subscribe to portfolio channel for balance updates
    std::string portfolio_subscription = create_portfolio_subscription();
    std::cout << "[DERIBIT_PMS] Subscribing to portfolio channel: " << portfolio_subscription << std::endl;
    
    // Note: Authentication and subscription messages are handled by the mock transport's automatic replay
    // For real WebSocket connections, the messages would be sent here
    
    // In mock mode, simulate authentication and subscription responses
    if (!custom_transport_) {
        std::string mock_auth_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
            R"(,"result":{"access_token":"mock_token","expires_in":3600}})";
        handle_websocket_message(mock_auth_response);
        
        std::string mock_portfolio_response = R"({"jsonrpc":"2.0","id":)" + std::to_string(request_id_++) + 
            R"(,"result":["user.portfolio.BTC","user.changes.any.any"]})";
        handle_websocket_message(mock_portfolio_response);
    }
    
    return true;
}

std::string DeribitPMS::create_auth_message() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "public/auth";
    
    Json::Value params;
    params["grant_type"] = "client_credentials";
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitPMS::create_portfolio_subscription() {
    Json::Value root;
    root["jsonrpc"] = "2.0";
    root["id"] = static_cast<int>(request_id_++);
    root["method"] = "private/subscribe";
    
    Json::Value params(Json::arrayValue);
    params.append("user.portfolio." + config_.currency);
    params.append("user.changes.any.any");
    
    root["params"] = params;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string DeribitPMS::generate_request_id() {
    return std::to_string(request_id_++);
}

} // namespace deribit
