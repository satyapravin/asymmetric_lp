#pragma once
#include "../utils/app_service/app_service.hpp"
#include "position_server_lib.hpp"
#include "../../utils/zmq/zmq_publisher.hpp"

namespace position_server {

/**
 * Position Server Service
 * 
 * Extends AppService to implement position management server functionality
 * This is part of the library layer for testing purposes
 */
class PositionServerService : public app_service::AppService {
public:
    PositionServerService();
    ~PositionServerService() = default;

protected:
    // AppService interface implementation
    bool configure_service() override;
    bool start_service() override;
    void stop_service() override;
    void print_service_stats() override;

private:
    std::unique_ptr<PositionServerLib> position_server_lib_;
    std::shared_ptr<ZmqPublisher> publisher_;
    
    std::string exchange_;
    std::string zmq_publish_endpoint_;
};

} // namespace position_server
