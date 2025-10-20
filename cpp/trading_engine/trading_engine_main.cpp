#include <iostream>
#include "trading_engine_service.hpp"

int main(int argc, char** argv) {
    trading_engine::TradingEngineService service;
    
    if (!service.initialize(argc, argv)) {
        return 1;
    }
    
    service.start();
    return 0;
}