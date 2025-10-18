#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "Running Asymmetric LP Test Suite" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Run doctest
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    
    int result = context.run();
    
    if (context.shouldExit()) {
        return result;
    }
    
    std::cout << "Test Suite Complete" << std::endl;
    return result;
}
