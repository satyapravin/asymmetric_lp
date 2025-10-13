#include "grvt_client_stub.hpp"
#include <iostream>
#include <iomanip>

void GrvtClientStub::submit_perp_order(const std::string& asset_symbol, double qty) {
  std::cout << std::fixed << std::setprecision(6)
            << "[GRVT] Submit perp order: symbol=" << asset_symbol
            << " qty=" << qty << std::endl;
}


