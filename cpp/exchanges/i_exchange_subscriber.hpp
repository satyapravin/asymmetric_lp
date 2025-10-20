#pragma once
#include "../proto/market_data.pb.h"
#include "websocket/i_websocket_transport.hpp"
#include <functional>

// Callback types for market data
using OrderbookCallback = std::function<void(const proto::OrderBookSnapshot& orderbook)>;
using TradeCallback = std::function<void(const proto::Trade& trade)>;

/**
 * IExchangeSubscriber - Market Data Subscriber Interface
 * 
 * Purpose: Market data via public WebSocket channels
 * Used by: Quote Server processes (one per exchange: quote_server_binance, etc.)
 * 
 * Flow: Exchange → WebSocket → Quote Server → ZMQ → Trader
 * 
 * Key Design:
 * - Public WebSocket for market data (no authentication)
 * - Configurable orderbook snapshots (top_n, frequency_ms)
 * - Real-time trade data
 * - Exchange-specific implementations: BinanceSubscriber, GrvtSubscriber, DeribitSubscriber
 */
class IExchangeSubscriber {
public:
    virtual ~IExchangeSubscriber() = default;
    
    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // Market data subscriptions (via WebSocket)
    virtual bool subscribe_orderbook(const std::string& symbol, int top_n, int frequency_ms) = 0;
    virtual bool subscribe_trades(const std::string& symbol) = 0;
    virtual bool unsubscribe(const std::string& symbol) = 0;
    
    // Real-time callbacks
    virtual void set_orderbook_callback(OrderbookCallback callback) = 0;
    virtual void set_trade_callback(TradeCallback callback) = 0;
    virtual void set_error_callback(std::function<void(const std::string&)> callback) = 0;
    
    // Testing interface - inject custom WebSocket transport
    virtual void set_websocket_transport(std::unique_ptr<websocket_transport::IWebSocketTransport> transport) = 0;
};