#pragma once
#include "../proto/position.pb.h"
#include <functional>

// Callback types for real-time updates
using PositionUpdateCallback = std::function<void(const proto::PositionUpdate& position)>;

/**
 * IExchangePMS - Position Management System Interface
 * 
 * Purpose: Position management via private WebSocket channels
 * Used by: Position Server processes (one per exchange: position_server_binance, etc.)
 * 
 * Flow: Exchange → WebSocket → Position Server → ZMQ → Trader
 * 
 * Key Design:
 * - WebSocket for real-time position and account updates
 * - Only callbacks, no query methods (live updates only)
 * - Auto-authentication on connect()
 * - Exchange-specific implementations: BinancePMS, GrvtPMS, DeribitPMS
 */
class IExchangePMS {
public:
    virtual ~IExchangePMS() = default;
    
    // Connection management
    virtual bool connect() = 0;  // Auto-authenticates
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Authentication
    virtual void set_auth_credentials(const std::string& api_key, const std::string& secret) = 0;
    virtual bool is_authenticated() const = 0;
    
    // Real-time callbacks only (no query methods)
    virtual void set_position_update_callback(PositionUpdateCallback callback) = 0;
};
