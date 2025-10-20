#include <iostream>
#include "position_server_service.hpp"

int main(int argc, char** argv) {
    position_server::PositionServerService service;
    
    if (!service.initialize(argc, argv)) {
        return 1;
    }
    
    service.start();
    return 0;
}