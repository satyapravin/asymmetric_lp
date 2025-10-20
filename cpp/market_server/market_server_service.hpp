#pragma once
#include "../utils/app_service/app_service.hpp"
#include "market_server_lib.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"

namespace market_server {

/**
 * Market Server Service
 * 
 * Extends AppService to implement market data server functionality
 * This is part of the library layer for testing purposes
 */
class MarketServerService : public app_service::AppService {
public:
    MarketServerService();
    ~MarketServerService() = default;

protected:
    // AppService interface implementation
    bool configure_service() override;
    bool start_service() override;
    void stop_service() override;
    void print_service_stats() override;

private:
    std::unique_ptr<MarketServerLib> market_server_lib_;
    std::shared_ptr<ZmqPublisher> publisher_;
    
    std::string exchange_;
    std::string symbol_;
    std::string zmq_publish_endpoint_;
};

} // namespace market_server
