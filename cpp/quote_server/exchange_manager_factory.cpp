#include "i_exchange_manager.hpp"
#include "exchange_manager.hpp"
#include "exchanges/deribit/deribit_manager.hpp"
#include <dlfcn.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

std::unique_ptr<IExchangeManager> ExchangeManagerFactory::create(const std::string& exchange_name,
                                                               const std::string& websocket_url) {
  std::string exchange_upper = exchange_name;
  std::transform(exchange_upper.begin(), exchange_upper.end(), exchange_upper.begin(), [](unsigned char c){ return std::toupper(c); });
  
  if (exchange_upper == "BINANCE") {
    // Try plugin path from env or default location
    const char* plugin_path = std::getenv("BINANCE_PLUGIN_PATH");
    void* handle = plugin_path ? dlopen(plugin_path, RTLD_NOW) : nullptr;
    if (handle) {
      using create_t = void* (*)(const char*);
      create_t create_fn = (create_t)dlsym(handle, "create_exchange_manager");
      if (create_fn) {
        IExchangeManager* mgr = (IExchangeManager*)create_fn(websocket_url.c_str());
        if (mgr) {
          return std::unique_ptr<IExchangeManager>(mgr);
        }
      }
    }
    // Fallback currently disabled to enforce plugin usage; could return nullptr to signal error
    return nullptr;
  }
  if (exchange_upper == "DERIBIT") {
    return std::make_unique<DeribitManager>(websocket_url);
  }
  // Fallback to generic
  return std::make_unique<ExchangeManager>(exchange_name, websocket_url);
}
