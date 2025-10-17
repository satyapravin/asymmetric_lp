#include "binance_manager.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include "../../utils/config/config.hpp"
#include <cstring>
#include "binance_lws_client.h"
#include <atomic>
#include <sstream>

BinanceManager::BinanceManager(const std::string& websocket_url)
  : websocket_url_(websocket_url) {}

BinanceManager::~BinanceManager() { stop(); }

static void on_open_thunk(void* user) { static_cast<BinanceManager*>(user)->handle_ws_open(); }
static void on_message_thunk(void* user, const char* data, int len) { static_cast<BinanceManager*>(user)->handle_ws_message(std::string(data, len)); }
static void on_close_thunk(void* user) { static_cast<BinanceManager*>(user)->handle_ws_close(); }

bool BinanceManager::start() {
  if (running_.load()) return true;
  running_.store(true);
  emit_connection(true);
  worker_ = std::thread([this]{
    // Build combined stream path: /stream?streams=s1/s2
    std::vector<std::string> streams;
    {
      std::lock_guard<std::mutex> lock(subs_mutex_);
      for (const auto& sym : subs_) {
        if (channels_.empty()) {
          streams.push_back(sym + "@depth" + std::to_string(book_depth_) + "@100ms");
          streams.push_back(sym + "@trade");
        } else {
          for (const auto& ch : channels_) streams.push_back(sym + "@" + ch);
        }
      }
    }
    std::string path = "/stream?streams=";
    for (size_t i = 0; i < streams.size(); ++i) {
      path += streams[i];
      if (i + 1 < streams.size()) path += "/";
    }

    std::string host = "fstream.binance.com";
    int port = 443;
    bool use_ssl = true;

    runflag_ = 1;
    BinanceLwsCallbacks cbs{&on_open_thunk, &on_message_thunk, &on_close_thunk};
    binance_lws_run(host.c_str(), port, use_ssl ? 1 : 0, path.c_str(), &runflag_, this, cbs);
  });
  return true;
}

void BinanceManager::stop() {
  if (!running_.load()) return;
  running_.store(false);
  runflag_ = 0;
  if (worker_.joinable()) worker_.join();
  emit_connection(false);
}

void BinanceManager::subscribe_symbol(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(subs_mutex_);
  subs_.push_back(symbol);
}

void BinanceManager::unsubscribe_symbol(const std::string& symbol) {
  subs_.erase(std::remove(subs_.begin(), subs_.end(), symbol), subs_.end());
}


extern "C" {
  BinanceManager* create_exchange_manager(const char* websocket_url) {
    return new BinanceManager(websocket_url ? std::string(websocket_url) : std::string());
  }
  void destroy_exchange_manager(BinanceManager* mgr) {
    delete mgr;
  }
}
