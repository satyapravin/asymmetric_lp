#pragma once
#include "../proto/order.pb.h"
#include "../proto/position.pb.h"
#include "../proto/acc_balance.pb.h"

/**
 * IExchangeDataFetcher - HTTP Data Fetcher Interface
 * 
 * Purpose: Startup state recovery via HTTP
 * Used by: Trader process during startup/crash recovery
 * 
 * Flow: Trader → HTTP → Exchange (one-time during startup)
 * 
 * Key Design:
 * - HTTP only, no WebSocket connections
 * - Used for getting current state after startup/crash
 * - Authentication via API keys
 * - Exchange-specific implementations: BinanceDataFetcher, GrvtDataFetcher, DeribitDataFetcher
 */
class IExchangeDataFetcher {
public:
    virtual ~IExchangeDataFetcher() = default;
    
    // Authentication
    virtual void set_auth_credentials(const std::string& api_key, const std::string& secret) = 0;
    virtual bool is_authenticated() const = 0;
    
    // Startup state recovery (HTTP only)
    virtual std::vector<proto::OrderEvent> get_open_orders() = 0;
    virtual std::vector<proto::PositionUpdate> get_positions() = 0;
    virtual std::vector<proto::AccountBalance> get_balances() = 0;
};