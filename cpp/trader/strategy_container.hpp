#pragma once
#include <memory>
#include <string>
#include "../proto/order.pb.h"
#include "../proto/market_data.pb.h"
#include "../proto/position.pb.h"
#include "mini_oms.hpp"

// Forward declarations for ZMQ adapters
class ZmqOMSAdapter;
class ZmqMDSAdapter;
class ZmqPMSAdapter;

/**
 * Strategy Container Interface
 * 
 * Simple interface for trader process to contain and manage a single strategy.
 * The trader process instantiates one strategy and routes events to it.
 */
class IStrategyContainer {
public:
    virtual ~IStrategyContainer() = default;
    
    // Strategy lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    
    // Event handlers
    virtual void on_market_data(const proto::OrderBookSnapshot& orderbook) = 0;
    virtual void on_order_event(const proto::OrderEvent& order_event) = 0;
    virtual void on_position_update(const proto::PositionUpdate& position) = 0;
    virtual void on_trade_execution(const proto::Trade& trade) = 0;
    
    // Configuration
    virtual void set_symbol(const std::string& symbol) = 0;
    virtual void set_exchange(const std::string& exchange) = 0;
    virtual const std::string& get_name() const = 0;
    
    // ZMQ adapter setup
    virtual void set_oms_adapter(std::shared_ptr<ZmqOMSAdapter> adapter) = 0;
    virtual void set_mds_adapter(std::shared_ptr<ZmqMDSAdapter> adapter) = 0;
    virtual void set_pms_adapter(std::shared_ptr<ZmqPMSAdapter> adapter) = 0;
};
