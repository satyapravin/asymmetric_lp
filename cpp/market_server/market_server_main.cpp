#include <iostream>
#include "market_server_service.hpp"

int main(int argc, char** argv) {
    market_server::MarketServerService service;
    
    if (!service.initialize(argc, argv)) {
        return 1;
    }
    
    service.start();
    return 0;
}