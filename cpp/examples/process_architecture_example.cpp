#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <signal.h>
#include <zmq.hpp>
#include <json/json.h>

// Process architecture example
// This demonstrates the communication flow between processes

namespace process_architecture {

// Message types for ZMQ communication
enum class MessageType {
    ORDER_REQUEST,
    ORDER_RESPONSE,
    ORDER_STATUS_UPDATE,
    TRADE_EXECUTION,
    MARKET_DATA_UPDATE,
    POSITION_UPDATE,
    BALANCE_UPDATE
};

// Base message structure
struct Message {
    MessageType type;
    std::string exchange;
    std::string symbol;
    uint64_t timestamp_us;
    std::string data;  // JSON payload
};

// Trader process simulation
class TraderProcess {
public:
    TraderProcess() {
        std::cout << "[TRADER] Initializing trader process..." << std::endl;
        
        // Initialize ZMQ context
        context_ = std::make_unique<zmq::context_t>(1);
        
        // Create publishers
        order_events_pub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        order_events_pub_->bind("tcp://127.0.0.1:6002");
        
        // Create subscribers
        quote_server_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        quote_server_sub_->connect("tcp://127.0.0.1:6001");
        quote_server_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        position_server_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        position_server_sub_->connect("tcp://127.0.0.1:6003");
        position_server_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        trading_engine_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        trading_engine_sub_->connect("tcp://127.0.0.1:7003");
        trading_engine_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    }
    
    void run() {
        std::cout << "[TRADER] Starting trader process..." << std::endl;
        
        // Start market data processing thread
        std::thread market_data_thread(&TraderProcess::process_market_data, this);
        
        // Start order processing thread
        std::thread order_thread(&TraderProcess::process_orders, this);
        
        // Main strategy loop
        strategy_loop();
        
        // Wait for threads
        market_data_thread.join();
        order_thread.join();
    }
    
    void send_order_request(const std::string& exchange, const std::string& symbol, 
                           const std::string& side, double qty, double price) {
        Json::Value order;
        order["type"] = "ORDER_REQUEST";
        order["exchange"] = exchange;
        order["symbol"] = symbol;
        order["side"] = side;
        order["qty"] = qty;
        order["price"] = price;
        order["timestamp_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, order);
        
        order_events_pub_->send(zmq::buffer(message), zmq::send_flags::dontwait);
        
        std::cout << "[TRADER] Sent order request: " << symbol << " " << side 
                  << " " << qty << "@" << price << std::endl;
    }

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> order_events_pub_;
    std::unique_ptr<zmq::socket_t> quote_server_sub_;
    std::unique_ptr<zmq::socket_t> position_server_sub_;
    std::unique_ptr<zmq::socket_t> trading_engine_sub_;
    
    void process_market_data() {
        std::cout << "[TRADER] Market data processing thread started" << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = quote_server_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[TRADER] Received market data: " << data << std::endl;
                
                // Process market data for strategy decisions
                process_market_data_update(data);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void process_orders() {
        std::cout << "[TRADER] Order processing thread started" << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = trading_engine_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[TRADER] Received order update: " << data << std::endl;
                
                // Process order updates
                process_order_update(data);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void strategy_loop() {
        std::cout << "[TRADER] Starting strategy loop..." << std::endl;
        
        // Simple market making strategy
        for (int i = 0; i < 10; ++i) {
            // Send buy order
            send_order_request("BINANCE", "BTCUSDT", "BUY", 0.001, 50000.0);
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Send sell order
            send_order_request("BINANCE", "BTCUSDT", "SELL", 0.001, 50010.0);
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "[TRADER] Strategy loop completed" << std::endl;
    }
    
    void process_market_data_update(const std::string& data) {
        // Process market data for trading decisions
    }
    
    void process_order_update(const std::string& data) {
        // Process order status updates
    }
};

// Quote Server process simulation
class QuoteServerProcess {
public:
    QuoteServerProcess(const std::string& exchange) : exchange_(exchange) {
        std::cout << "[QUOTE_SERVER_" << exchange << "] Initializing quote server..." << std::endl;
        
        context_ = std::make_unique<zmq::context_t>(1);
        
        // Create publisher
        market_data_pub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        market_data_pub_->bind("tcp://127.0.0.1:6001");
        
        // Create subscriber
        trader_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        trader_sub_->connect("tcp://127.0.0.1:7001");
        trader_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    }
    
    void run() {
        std::cout << "[QUOTE_SERVER_" << exchange_ << "] Starting quote server..." << std::endl;
        
        // Simulate market data publishing
        std::thread market_data_thread(&QuoteServerProcess::publish_market_data, this);
        
        // Process trader messages
        std::thread trader_thread(&QuoteServerProcess::process_trader_messages, this);
        
        // Wait for threads
        market_data_thread.join();
        trader_thread.join();
    }

private:
    std::string exchange_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> market_data_pub_;
    std::unique_ptr<zmq::socket_t> trader_sub_;
    
    void publish_market_data() {
        std::cout << "[QUOTE_SERVER_" << exchange_ << "] Publishing market data..." << std::endl;
        
        for (int i = 0; i < 20; ++i) {
            Json::Value market_data;
            market_data["type"] = "MARKET_DATA_UPDATE";
            market_data["exchange"] = exchange_;
            market_data["symbol"] = "BTCUSDT";
            market_data["bid"] = 50000.0 + i;
            market_data["ask"] = 50001.0 + i;
            market_data["timestamp_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            Json::StreamWriterBuilder builder;
            std::string message = Json::writeString(builder, market_data);
            
            market_data_pub_->send(zmq::buffer(message), zmq::send_flags::dontwait);
            
            std::cout << "[QUOTE_SERVER_" << exchange_ << "] Published market data: " 
                      << market_data["bid"] << "/" << market_data["ask"] << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
    void process_trader_messages() {
        std::cout << "[QUOTE_SERVER_" << exchange_ << "] Processing trader messages..." << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = trader_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[QUOTE_SERVER_" << exchange_ << "] Received trader message: " 
                          << data << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

// Trading Engine process simulation
class TradingEngineProcess {
public:
    TradingEngineProcess(const std::string& exchange) : exchange_(exchange) {
        std::cout << "[TRADING_ENGINE_" << exchange << "] Initializing trading engine..." << std::endl;
        
        context_ = std::make_unique<zmq::context_t>(1);
        
        // Create publishers
        order_events_pub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        order_events_pub_->bind("tcp://127.0.0.1:7003");
        
        trade_events_pub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        trade_events_pub_->bind("tcp://127.0.0.1:6017");
        
        // Create subscribers
        trader_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        trader_sub_->connect("tcp://127.0.0.1:7003");
        trader_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        position_server_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        position_server_sub_->connect("tcp://127.0.0.1:7004");
        position_server_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    }
    
    void run() {
        std::cout << "[TRADING_ENGINE_" << exchange_ << "] Starting trading engine..." << std::endl;
        
        // Process trader messages
        std::thread trader_thread(&TradingEngineProcess::process_trader_messages, this);
        
        // Process position server messages
        std::thread position_thread(&TradingEngineProcess::process_position_messages, this);
        
        // Simulate order processing
        std::thread order_thread(&TradingEngineProcess::process_orders, this);
        
        // Wait for threads
        trader_thread.join();
        position_thread.join();
        order_thread.join();
    }

private:
    std::string exchange_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> order_events_pub_;
    std::unique_ptr<zmq::socket_t> trade_events_pub_;
    std::unique_ptr<zmq::socket_t> trader_sub_;
    std::unique_ptr<zmq::socket_t> position_server_sub_;
    
    void process_trader_messages() {
        std::cout << "[TRADING_ENGINE_" << exchange_ << "] Processing trader messages..." << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = trader_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[TRADING_ENGINE_" << exchange_ << "] Received trader message: " 
                          << data << std::endl;
                
                // Process order request
                process_order_request(data);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void process_position_messages() {
        std::cout << "[TRADING_ENGINE_" << exchange_ << "] Processing position messages..." << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = position_server_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[TRADING_ENGINE_" << exchange_ << "] Received position message: " 
                          << data << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void process_orders() {
        std::cout << "[TRADING_ENGINE_" << exchange_ << "] Processing orders..." << std::endl;
        
        // Simulate order processing and sending responses
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Send order response
            Json::Value order_response;
            order_response["type"] = "ORDER_RESPONSE";
            order_response["exchange"] = exchange_;
            order_response["cl_ord_id"] = "order_" + std::to_string(i);
            order_response["status"] = "FILLED";
            order_response["timestamp_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            Json::StreamWriterBuilder builder;
            std::string message = Json::writeString(builder, order_response);
            
            order_events_pub_->send(zmq::buffer(message), zmq::send_flags::dontwait);
            
            std::cout << "[TRADING_ENGINE_" << exchange_ << "] Sent order response: " 
                      << order_response["cl_ord_id"] << " " << order_response["status"] << std::endl;
        }
    }
    
    void process_order_request(const std::string& data) {
        // Process order request from trader
        std::cout << "[TRADING_ENGINE_" << exchange_ << "] Processing order request..." << std::endl;
    }
};

// Position Server process simulation
class PositionServerProcess {
public:
    PositionServerProcess(const std::string& exchange) : exchange_(exchange) {
        std::cout << "[POSITION_SERVER_" << exchange << "] Initializing position server..." << std::endl;
        
        context_ = std::make_unique<zmq::context_t>(1);
        
        // Create publisher
        position_events_pub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        position_events_pub_->bind("tcp://127.0.0.1:6003");
        
        // Create subscribers
        trader_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        trader_sub_->connect("tcp://127.0.0.1:7002");
        trader_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        trading_engine_sub_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        trading_engine_sub_->connect("tcp://127.0.0.1:7004");
        trading_engine_sub_->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    }
    
    void run() {
        std::cout << "[POSITION_SERVER_" << exchange_ << "] Starting position server..." << std::endl;
        
        // Publish position updates
        std::thread position_thread(&PositionServerProcess::publish_position_updates, this);
        
        // Process trader messages
        std::thread trader_thread(&PositionServerProcess::process_trader_messages, this);
        
        // Process trading engine messages
        std::thread engine_thread(&PositionServerProcess::process_engine_messages, this);
        
        // Wait for threads
        position_thread.join();
        trader_thread.join();
        engine_thread.join();
    }

private:
    std::string exchange_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> position_events_pub_;
    std::unique_ptr<zmq::socket_t> trader_sub_;
    std::unique_ptr<zmq::socket_t> trading_engine_sub_;
    
    void publish_position_updates() {
        std::cout << "[POSITION_SERVER_" << exchange_ << "] Publishing position updates..." << std::endl;
        
        for (int i = 0; i < 10; ++i) {
            Json::Value position_update;
            position_update["type"] = "POSITION_UPDATE";
            position_update["exchange"] = exchange_;
            position_update["symbol"] = "BTCUSDT";
            position_update["qty"] = 0.001 * i;
            position_update["avg_price"] = 50000.0 + i * 10;
            position_update["timestamp_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            Json::StreamWriterBuilder builder;
            std::string message = Json::writeString(builder, position_update);
            
            position_events_pub_->send(zmq::buffer(message), zmq::send_flags::dontwait);
            
            std::cout << "[POSITION_SERVER_" << exchange_ << "] Published position update: " 
                      << position_update["qty"] << "@" << position_update["avg_price"] << std::endl;
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    void process_trader_messages() {
        std::cout << "[POSITION_SERVER_" << exchange_ << "] Processing trader messages..." << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = trader_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[POSITION_SERVER_" << exchange_ << "] Received trader message: " 
                          << data << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void process_engine_messages() {
        std::cout << "[POSITION_SERVER_" << exchange_ << "] Processing engine messages..." << std::endl;
        
        while (true) {
            zmq::message_t message;
            auto result = trading_engine_sub_->recv(message, zmq::recv_flags::dontwait);
            
            if (result) {
                std::string data(static_cast<char*>(message.data()), message.size());
                std::cout << "[POSITION_SERVER_" << exchange_ << "] Received engine message: " 
                          << data << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

} // namespace process_architecture

int main() {
    std::cout << "=== Process Architecture Example ===" << std::endl;
    std::cout << "This demonstrates the communication flow between trading processes" << std::endl;
    
    // Create processes
    process_architecture::TraderProcess trader;
    process_architecture::QuoteServerProcess quote_server_binance("BINANCE");
    process_architecture::TradingEngineProcess trading_engine_binance("BINANCE");
    process_architecture::PositionServerProcess position_server_binance("BINANCE");
    
    // Start processes in separate threads
    std::thread trader_thread([&trader]() { trader.run(); });
    std::thread quote_thread([&quote_server_binance]() { quote_server_binance.run(); });
    std::thread engine_thread([&trading_engine_binance]() { trading_engine_binance.run(); });
    std::thread position_thread([&position_server_binance]() { position_server_binance.run(); });
    
    // Wait for processes to complete
    trader_thread.join();
    quote_thread.join();
    engine_thread.join();
    position_thread.join();
    
    std::cout << "=== Process Architecture Example Completed ===" << std::endl;
    return 0;
}
