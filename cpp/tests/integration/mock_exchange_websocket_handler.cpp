#include "mock_exchange_websocket_handler.hpp"
#include <iostream>
#include <filesystem>
#include <random>
#include <chrono>

namespace testing {

MockExchangeWebSocketHandler::MockExchangeWebSocketHandler(const std::string& exchange_name, const std::string& data_path)
    : exchange_name_(exchange_name), data_path_(data_path) {
    load_test_data();
}

MockExchangeWebSocketHandler::~MockExchangeWebSocketHandler() {
    disconnect();
}

bool MockExchangeWebSocketHandler::connect() {
    if (connected_.load()) {
        return true;
    }

    std::cout << "[MOCK_WS] Connecting to " << exchange_name_ << " WebSocket..." << std::endl;

    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    connected_.store(true);
    authenticated_.store(true);

    std::cout << "[MOCK_WS] Connected to " << exchange_name_ << " WebSocket" << std::endl;

    // Start message simulation if enabled
    if (simulation_mode_) {
        start_message_simulation();
    }

    return true;
}

void MockExchangeWebSocketHandler::disconnect() {
    if (!connected_.load()) {
        return;
    }

    std::cout << "[MOCK_WS] Disconnecting from " << exchange_name_ << " WebSocket..." << std::endl;

    stop_message_simulation();
    connected_.store(false);
    authenticated_.store(false);

    std::cout << "[MOCK_WS] Disconnected from " << exchange_name_ << " WebSocket" << std::endl;
}

bool MockExchangeWebSocketHandler::is_connected() const {
    return connected_.load();
}

void MockExchangeWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    std::cout << "[MOCK_WS] Setting auth credentials for " << exchange_name_ << std::endl;
    // In real implementation, this would store credentials
}

bool MockExchangeWebSocketHandler::is_authenticated() const {
    return authenticated_.load();
}

void MockExchangeWebSocketHandler::set_order_event_callback(OrderEventCallback callback) {
    order_event_callback_ = callback;
    std::cout << "[MOCK_WS] Order event callback set for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::set_trade_execution_callback(TradeExecutionCallback callback) {
    trade_execution_callback_ = callback;
    std::cout << "[MOCK_WS] Trade execution callback set for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::set_orderbook_callback(OrderBookCallback callback) {
    orderbook_callback_ = callback;
    std::cout << "[MOCK_WS] Orderbook callback set for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::set_trade_callback(TradeCallback callback) {
    trade_callback_ = callback;
    std::cout << "[MOCK_WS] Trade callback set for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::set_position_update_callback(PositionUpdateCallback callback) {
    position_update_callback_ = callback;
    std::cout << "[MOCK_WS] Position update callback set for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::set_account_balance_update_callback(AccountBalanceUpdateCallback callback) {
    balance_update_callback_ = callback;
    std::cout << "[MOCK_WS] Account balance update callback set for " << exchange_name_ << std::endl;
}

bool MockExchangeWebSocketHandler::send_order(const std::string& cl_ord_id, const std::string& symbol, 
                                             proto::Side side, proto::OrderType type, double qty, double price) {
    if (!connected_.load()) {
        std::cerr << "[MOCK_WS] Not connected to " << exchange_name_ << std::endl;
        return false;
    }

    std::cout << "[MOCK_WS] Sending order: " << cl_ord_id << " " << symbol << " " 
              << static_cast<int>(side) << " " << static_cast<int>(type) << " " << qty << " " << price << std::endl;

    // Store order info for realistic responses
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        OrderInfo order_info;
        order_info.cl_ord_id = cl_ord_id;
        order_info.symbol = symbol;
        order_info.side = side;
        order_info.type = type;
        order_info.qty = qty;
        order_info.price = price;
        order_info.timestamp = std::chrono::system_clock::now();
        order_info.status = proto::OrderStatus::NEW;
        
        pending_orders_[cl_ord_id] = order_info;
    }

    // Simulate realistic order response timing
    std::thread([this, cl_ord_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (rand() % 100)));
        simulate_realistic_order_response(cl_ord_id);
    }).detach();

    return true;
}

bool MockExchangeWebSocketHandler::cancel_order(const std::string& cl_ord_id) {
    if (!connected_.load()) {
        return false;
    }

    std::cout << "[MOCK_WS] Cancelling order: " << cl_ord_id << std::endl;

    // Simulate cancel response
    std::thread([this, cl_ord_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30 + (rand() % 50)));
        
        if (order_event_callback_) {
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(cl_ord_id);
            order_event.set_status(proto::OrderStatus::CANCELLED);
            order_event.set_exchange(exchange_name_);
            order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            order_event_callback_(order_event);
        }
    }).detach();

    return true;
}

bool MockExchangeWebSocketHandler::modify_order(const std::string& cl_ord_id, double new_price, double new_qty) {
    if (!connected_.load()) {
        return false;
    }

    std::cout << "[MOCK_WS] Modifying order: " << cl_ord_id << " price=" << new_price << " qty=" << new_qty << std::endl;

    // Update stored order info
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = pending_orders_.find(cl_ord_id);
        if (it != pending_orders_.end()) {
            it->second.price = new_price;
            it->second.qty = new_qty;
        }
    }

    // Simulate modify response
    std::thread([this, cl_ord_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(40 + (rand() % 60)));
        
        if (order_event_callback_) {
            proto::OrderEvent order_event;
            order_event.set_cl_ord_id(cl_ord_id);
            order_event.set_status(proto::OrderStatus::NEW); // Modified orders typically get NEW status
            order_event.set_exchange(exchange_name_);
            order_event.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            order_event_callback_(order_event);
        }
    }).detach();

    return true;
}

void MockExchangeWebSocketHandler::load_test_data() {
    std::cout << "[MOCK_WS] Loading test data from " << data_path_ << std::endl;

    // Load orderbook messages
    std::string orderbook_dir = data_path_ + "/websocket/";
    for (int i = 1; i <= 5; ++i) {
        std::string filename = orderbook_dir + "orderbook_snapshot_message" + (i == 1 ? "" : "_" + std::to_string(i)) + ".json";
        std::string content = load_json_file(filename);
        if (!content.empty()) {
            orderbook_messages_.push_back(content);
        }
    }

    // Load trade messages
    std::vector<std::string> trade_files = {
        "trade_message.json",
        "trade_message_large.json", 
        "trade_message_sell.json"
    };
    
    for (const auto& filename : trade_files) {
        std::string content = load_json_file(orderbook_dir + filename);
        if (!content.empty()) {
            trade_messages_.push_back(content);
        }
    }

    // Load order update templates
    std::vector<std::string> order_template_files = {
        "order_update_message_ack.json",
        "order_update_message_reject.json",
        "order_update_message_cancelled.json",
        "order_update_message_cancel_reject.json",
        "order_update_message_partial_fill.json",
        "order_update_message_filled.json"
    };
    
    for (const auto& filename : order_template_files) {
        std::string content = load_json_file(orderbook_dir + filename);
        if (!content.empty()) {
            order_update_templates_.push_back(content);
        }
    }

    // Load position and balance templates
    position_update_template_ = load_json_file(data_path_ + "/websocket/account_update_message.json");
    balance_update_template_ = load_json_file(data_path_ + "/websocket/account_update_message.json");

    std::cout << "[MOCK_WS] Loaded " << orderbook_messages_.size() << " orderbook messages, "
              << trade_messages_.size() << " trade messages, "
              << order_update_templates_.size() << " order templates" << std::endl;
}

void MockExchangeWebSocketHandler::start_message_simulation() {
    if (simulation_running_.load()) {
        return;
    }

    simulation_running_.store(true);

    // Start simulation threads
    market_data_thread_ = std::thread(&MockExchangeWebSocketHandler::market_data_simulation_loop, this);
    order_response_thread_ = std::thread(&MockExchangeWebSocketHandler::order_response_simulation_loop, this);
    position_update_thread_ = std::thread(&MockExchangeWebSocketHandler::position_update_simulation_loop, this);
    balance_update_thread_ = std::thread(&MockExchangeWebSocketHandler::balance_update_simulation_loop, this);

    std::cout << "[MOCK_WS] Started message simulation for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::stop_message_simulation() {
    if (!simulation_running_.load()) {
        return;
    }

    simulation_running_.store(false);

    // Wait for threads to finish
    if (market_data_thread_.joinable()) {
        market_data_thread_.join();
    }
    if (order_response_thread_.joinable()) {
        order_response_thread_.join();
    }
    if (position_update_thread_.joinable()) {
        position_update_thread_.join();
    }
    if (balance_update_thread_.joinable()) {
        balance_update_thread_.join();
    }

    std::cout << "[MOCK_WS] Stopped message simulation for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::market_data_simulation_loop() {
    std::cout << "[MOCK_WS] Market data simulation started for " << exchange_name_ << std::endl;

    size_t orderbook_index = 0;
    size_t trade_index = 0;

    while (simulation_running_.load()) {
        // Simulate orderbook updates
        if (!orderbook_messages_.empty() && orderbook_callback_) {
            const std::string& message = orderbook_messages_[orderbook_index % orderbook_messages_.size()];
            
            proto::OrderBookSnapshot snapshot;
            if (parse_orderbook_message(message, snapshot)) {
                orderbook_callback_(snapshot);
            }
            
            orderbook_index++;
        }

        // Simulate trade updates
        if (!trade_messages_.empty() && trade_callback_) {
            const std::string& message = trade_messages_[trade_index % trade_messages_.size()];
            
            proto::Trade trade;
            if (parse_trade_message(message, trade)) {
                trade_callback_(trade);
            }
            
            trade_index++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(message_delay_ms_));
    }

    std::cout << "[MOCK_WS] Market data simulation stopped for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::order_response_simulation_loop() {
    std::cout << "[MOCK_WS] Order response simulation started for " << exchange_name_ << std::endl;

    while (simulation_running_.load()) {
        // This loop handles order responses triggered by send_order()
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[MOCK_WS] Order response simulation stopped for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::position_update_simulation_loop() {
    std::cout << "[MOCK_WS] Position update simulation started for " << exchange_name_ << std::endl;

    while (simulation_running_.load()) {
        if (position_update_callback_ && !position_update_template_.empty()) {
            proto::PositionUpdate position;
            if (parse_position_update(position_update_template_, position)) {
                position_update_callback_(position);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // Position updates every 5 seconds
    }

    std::cout << "[MOCK_WS] Position update simulation stopped for " << exchange_name_ << std::endl;
}

void MockExchangeWebSocketHandler::balance_update_simulation_loop() {
    std::cout << "[MOCK_WS] Balance update simulation started for " << exchange_name_ << std::endl;

    while (simulation_running_.load()) {
        if (balance_update_callback_ && !balance_update_template_.empty()) {
            proto::AccountBalanceUpdate balance;
            if (parse_balance_update(balance_update_template_, balance)) {
                balance_update_callback_(balance);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10000)); // Balance updates every 10 seconds
    }

    std::cout << "[MOCK_WS] Balance update simulation stopped for " << exchange_name_ << std::endl;
}

std::string MockExchangeWebSocketHandler::load_json_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[MOCK_WS] Failed to open file: " << filename << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool MockExchangeWebSocketHandler::parse_orderbook_message(const std::string& json_data, proto::OrderBookSnapshot& snapshot) {
    try {
        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(json_data, root)) {
            std::cerr << "[MOCK_WS] Failed to parse orderbook JSON" << std::endl;
            return false;
        }

        // Parse Binance depthUpdate format
        snapshot.set_symbol(root["s"].asString());
        snapshot.set_timestamp_us(root["E"].asUInt64());

        // Parse bids
        if (root.isMember("b") && root["b"].isArray()) {
            for (const auto& bid : root["b"]) {
                auto* level = snapshot.add_bids();
                level->set_price(std::stod(bid[0].asString()));
                level->set_qty(std::stod(bid[1].asString()));
            }
        }

        // Parse asks
        if (root.isMember("a") && root["a"].isArray()) {
            for (const auto& ask : root["a"]) {
                auto* level = snapshot.add_asks();
                level->set_price(std::stod(ask[0].asString()));
                level->set_qty(std::stod(ask[1].asString()));
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOCK_WS] Error parsing orderbook message: " << e.what() << std::endl;
        return false;
    }
}

bool MockExchangeWebSocketHandler::parse_trade_message(const std::string& json_data, proto::Trade& trade) {
    try {
        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(json_data, root)) {
            std::cerr << "[MOCK_WS] Failed to parse trade JSON" << std::endl;
            return false;
        }

        // Parse Binance trade format
        trade.set_symbol(root["s"].asString());
        trade.set_price(std::stod(root["p"].asString()));
        trade.set_qty(std::stod(root["q"].asString()));
        trade.set_timestamp_us(root["E"].asUInt64());
        trade.set_side(root["m"].asBool() ? proto::Side::SELL : proto::Side::BUY);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOCK_WS] Error parsing trade message: " << e.what() << std::endl;
        return false;
    }
}

bool MockExchangeWebSocketHandler::parse_order_update(const std::string& json_data, proto::OrderEvent& order_event) {
    try {
        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(json_data, root)) {
            std::cerr << "[MOCK_WS] Failed to parse order update JSON" << std::endl;
            return false;
        }

        // Parse Binance executionReport format
        order_event.set_cl_ord_id(root["c"].asString());
        order_event.set_symbol(root["s"].asString());
        order_event.set_qty(std::stod(root["q"].asString()));
        order_event.set_price(std::stod(root["p"].asString()));
        order_event.set_timestamp_us(root["E"].asUInt64());
        order_event.set_exchange(exchange_name_);

        // Map Binance order status to our enum
        std::string status_str = root["X"].asString();
        if (status_str == "NEW") {
            order_event.set_status(proto::OrderStatus::NEW);
        } else if (status_str == "FILLED") {
            order_event.set_status(proto::OrderStatus::FILLED);
        } else if (status_str == "PARTIALLY_FILLED") {
            order_event.set_status(proto::OrderStatus::PARTIALLY_FILLED);
        } else if (status_str == "CANCELED") {
            order_event.set_status(proto::OrderStatus::CANCELLED);
        } else if (status_str == "REJECTED") {
            order_event.set_status(proto::OrderStatus::REJECTED);
        } else if (status_str == "CANCEL_REJECT") {
            order_event.set_status(proto::OrderStatus::CANCELLED);
        }

        // Set filled quantity
        if (root.isMember("z")) {
            order_event.set_filled_qty(std::stod(root["z"].asString()));
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOCK_WS] Error parsing order update: " << e.what() << std::endl;
        return false;
    }
}

bool MockExchangeWebSocketHandler::parse_position_update(const std::string& json_data, proto::PositionUpdate& position) {
    try {
        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(json_data, root)) {
            std::cerr << "[MOCK_WS] Failed to parse position update JSON" << std::endl;
            return false;
        }

        // Parse Binance account update format
        position.set_exch(exchange_name_);
        position.set_timestamp_us(root["E"].asUInt64());

        if (root.isMember("a") && root["a"].isArray()) {
            for (const auto& pos : root["a"]) {
                position.set_symbol(pos["s"].asString());
                position.set_qty(std::stod(pos["pa"].asString()));
                position.set_avg_price(std::stod(pos["ep"].asString()));
                break; // Take first position for simplicity
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOCK_WS] Error parsing position update: " << e.what() << std::endl;
        return false;
    }
}

bool MockExchangeWebSocketHandler::parse_balance_update(const std::string& json_data, proto::AccountBalanceUpdate& balance) {
    try {
        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(json_data, root)) {
            std::cerr << "[MOCK_WS] Failed to parse balance update JSON" << std::endl;
            return false;
        }

        balance.set_timestamp_us(root["E"].asUInt64());

        if (root.isMember("B") && root["B"].isArray()) {
            for (const auto& bal : root["B"]) {
                auto* acc_balance = balance.add_balances();
                acc_balance->set_exch(exchange_name_);
                acc_balance->set_instrument(bal["a"].asString());
                acc_balance->set_balance(std::stod(bal["wb"].asString()));
                acc_balance->set_available(std::stod(bal["cw"].asString()));
                acc_balance->set_locked(std::stod(bal["wb"].asString()) - std::stod(bal["cw"].asString()));
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOCK_WS] Error parsing balance update: " << e.what() << std::endl;
        return false;
    }
}

void MockExchangeWebSocketHandler::simulate_realistic_order_response(const std::string& cl_ord_id) {
    if (!order_event_callback_) {
        return;
    }

    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = pending_orders_.find(cl_ord_id);
    if (it == pending_orders_.end()) {
        return;
    }

    const OrderInfo& order_info = it->second;

    // Simulate realistic order response sequence
    std::vector<std::string> response_sequence = {"ack", "partial_fill", "filled"};
    
    // Randomly choose response type
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, response_sequence.size() - 1);
    std::string response_type = response_sequence[dis(gen)];

    // Find appropriate template
    std::string template_json;
    for (const auto& template_str : order_update_templates_) {
        if (template_str.find("\"" + response_type + "\"") != std::string::npos ||
            template_str.find(response_type) != std::string::npos) {
            template_json = template_str;
            break;
        }
    }

    if (!template_json.empty()) {
        std::string populated_json = replace_order_placeholders(template_json, cl_ord_id, 
                                                               order_info.symbol, order_info.side, 
                                                               order_info.type, order_info.qty, order_info.price);
        
        proto::OrderEvent order_event;
        if (parse_order_update(populated_json, order_event)) {
            order_event_callback_(order_event);
        }
    }
}

std::string MockExchangeWebSocketHandler::replace_order_placeholders(const std::string& template_json, 
                                                                   const std::string& cl_ord_id,
                                                                   const std::string& symbol,
                                                                   proto::Side side,
                                                                   proto::OrderType type,
                                                                   double qty,
                                                                   double price) {
    std::string result = template_json;
    
    // Replace placeholders
    size_t pos = result.find("ORDER_ID_PLACEHOLDER");
    if (pos != std::string::npos) {
        result.replace(pos, 19, cl_ord_id);
    }
    
    pos = result.find("BTCUSDT");
    if (pos != std::string::npos) {
        result.replace(pos, 7, symbol);
    }
    
    // Replace side
    std::string side_str = (side == proto::Side::BUY) ? "BUY" : "SELL";
    pos = result.find("\"BUY\"");
    if (pos != std::string::npos) {
        result.replace(pos, 5, "\"" + side_str + "\"");
    }
    
    // Replace quantity and price
    pos = result.find("0.10000000");
    if (pos != std::string::npos) {
        std::string qty_str = std::to_string(qty);
        result.replace(pos, 10, qty_str);
    }
    
    pos = result.find("50000.00000000");
    if (pos != std::string::npos) {
        std::string price_str = std::to_string(price);
        result.replace(pos, 14, price_str);
    }
    
    return result;
}

std::string MockExchangeWebSocketHandler::generate_exchange_order_id() {
    return "EXCH_" + std::to_string(order_counter_.fetch_add(1));
}

} // namespace testing
