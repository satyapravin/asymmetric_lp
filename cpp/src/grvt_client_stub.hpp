#pragma once
#include <string>

class GrvtClientStub {
public:
  void submit_perp_order(const std::string& asset_symbol, double qty);
};


