#pragma once
#include "../utils/app_service/app_service.hpp"
#include "trading_engine_lib.hpp"
#include <memory>

namespace trading_engine {

/**
 * Trading Engine Service
 * 
 * Extends AppService to provide trading engine specific functionality.
 * This service manages the trading engine library and provides
 * process management capabilities.
 */
class TradingEngineService : public app_service::AppService {
public:
    TradingEngineService();
    ~TradingEngineService();

    // AppService interface
    bool configure_service() override;
    bool start_service() override;
    void stop_service() override;
    void print_service_stats() override;

private:
    std::unique_ptr<TradingEngineLib> trading_engine_lib_;
};

} // namespace trading_engine
