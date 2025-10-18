#pragma once
#include "i_exchange_handler.hpp"
#include "../utils/handlers/http/i_http_handler.hpp"
#include "../utils/handlers/websocket/i_websocket_handler.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <functional>
#include <vector>
#include <cstdint>

// Exchange-specific configuration
struct ExchangeConfig {
    std::string name;
    std::string api_key;
    std::string api_secret;
    std::string base_url;
    std::string websocket_url;
    bool testnet_mode{false};
    int timeout_ms{5000};
    std::map<std::string, std::string> custom_params;
};

// Exchange handler that uses HTTP and WebSocket handlers
class ExchangeHandler : public IExchangeHandler {
public:
    ExchangeHandler(const ExchangeConfig& config);
    ~ExchangeHandler() override;
    
    // IExchangeHandler interface
    bool start() override;
    void stop() override;
    bool is_connected() const override { return connected_.load(); }
    
    bool send_order(const Order& order) override;
    bool cancel_order(const std::string& client_order_id) override;
    bool modify_order(const Order& order) override;
    
    std::vector<Order> get_open_orders() override;
    Order get_order_status(const std::string& client_order_id) override;
    
    void set_order_event_callback(OrderEventCallback callback) override { order_event_callback_ = callback; }
    
    // Exchange-specific configuration
    void set_api_key(const std::string& key) override { config_.api_key = key; }
    void set_secret_key(const std::string& secret) override { config_.api_secret = secret; }
    void set_passphrase(const std::string& passphrase) override { config_.passphrase = passphrase; }
    void set_sandbox_mode(bool enabled) override { config_.testnet_mode = enabled; }
    
    std::string get_exchange_name() const override { return config_.name; }
    
    // Enhanced methods
    void set_http_handler(std::unique_ptr<IHttpHandler> handler);
    void set_websocket_handler(std::unique_ptr<IWebSocketHandler> handler);
    
    // Exchange-specific methods (to be implemented by derived classes)
    virtual std::string create_order_payload(const Order& order) = 0;
    virtual std::string create_cancel_payload(const std::string& client_order_id) = 0;
    virtual std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) = 0;
    virtual void handle_websocket_message(const std::string& message) = 0;
    virtual void handle_order_update(const std::string& message) = 0;
    virtual void handle_account_update(const std::string& message) = 0;

protected:
    // HTTP request helpers
    HttpResponse make_http_request(const std::string& method, const std::string& endpoint, 
                                  const std::string& body = "", bool authenticated = false);
    
    // WebSocket helpers
    bool send_websocket_message(const std::string& message);
    
    // Order management
    void update_order_status(const std::string& client_order_id, OrderStatus status, 
                           double filled_qty = 0.0, double avg_price = 0.0);
    
    // Configuration
    ExchangeConfig config_;
    
    // Handlers
    std::unique_ptr<IHttpHandler> http_handler_;
    std::unique_ptr<IWebSocketHandler> websocket_handler_;
    
    // State
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    // Order tracking
    std::mutex orders_mutex_;
    std::map<std::string, Order> active_orders_;
    
    // Callbacks
    OrderEventCallback order_event_callback_;
    
    // Additional fields for derived classes
    std::string passphrase_;
};

// Binance-specific exchange handler
class BinanceHandler : public ExchangeHandler {
public:
    BinanceHandler(const ExchangeConfig& config);
    
    std::string create_order_payload(const Order& order) override;
    std::string create_cancel_payload(const std::string& client_order_id) override;
    std::string create_auth_headers(const std::string& method, const std::string& endpoint, const std::string& body) override;
    
    void handle_websocket_message(const std::string& message) override;
    void handle_order_update(const std::string& message) override;
    void handle_account_update(const std::string& message) override;

private:
    std::string generate_signature(const std::string& query_string);
    std::string create_listen_key();
    void refresh_listen_key();
    
    std::string listen_key_;
    std::thread listen_key_refresh_thread_;
    std::atomic<bool> refresh_running_{false};
};
