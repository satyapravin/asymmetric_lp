#pragma once
#include "../proto/order.pb.h"
#include "websocket/i_websocket_transport.hpp"
#include <functional>
#include <memory>

// Callback types for real-time updates
using OrderStatusCallback = std::function<void(const proto::OrderEvent& order_event)>;

/**
 * IExchangeOMS - Order Management System Interface
 * 
 * Purpose: Order management via private WebSocket channels
 * Used by: Trading Engine processes (one per exchange: trading_engine_binance, trading_engine_grvt, etc.)
 * 
 * Flow: Trader → ZMQ → Trading Engine → WebSocket → Exchange
 * 
 * Key Design:
 * - WebSocket for live order operations (place, cancel, replace)
 * - Real-time order status updates via callbacks
 * - Auto-authentication on connect()
 * - Exchange-specific implementations: BinanceOMS, GrvtOMS, DeribitOMS
 */
class IExchangeOMS {
public:
    virtual ~IExchangeOMS() = default;
    
    // Connection management
    virtual bool connect() = 0;  // Auto-authenticates
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Authentication
    virtual void set_auth_credentials(const std::string& api_key, const std::string& secret) = 0;
    virtual bool is_authenticated() const = 0;
    
    // Order management (via WebSocket)
    virtual bool cancel_order(const std::string& cl_ord_id, const std::string& exch_ord_id) = 0;
    virtual bool replace_order(const std::string& cl_ord_id, const proto::OrderRequest& new_order) = 0;
    virtual proto::OrderEvent get_order_status(const std::string& cl_ord_id, const std::string& exch_ord_id) = 0;
    
    // Specific order types (via WebSocket)
    virtual bool place_market_order(const std::string& symbol, const std::string& side, double quantity) = 0;
    virtual bool place_limit_order(const std::string& symbol, const std::string& side, double quantity, double price) = 0;
    
    // Real-time callbacks
    virtual void set_order_status_callback(OrderStatusCallback callback) = 0;
    
    // WebSocket transport injection for testing
    virtual void set_websocket_transport(std::shared_ptr<websocket_transport::IWebSocketTransport> transport) = 0;
};