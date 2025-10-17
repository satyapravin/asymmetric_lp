#include "deribit_manager.hpp"
#include <chrono>
#include <thread>
#include <algorithm>

DeribitManager::DeribitManager(const std::string& websocket_url)
  : websocket_url_(websocket_url) {}

DeribitManager::~DeribitManager() { stop(); }

bool DeribitManager::start() {
  if (running_.load()) return true;
  running_.store(true);
  emit_connection(true);
  worker_ = std::thread([this]{ run_mock(); });
  return true;
}

void DeribitManager::stop() {
  if (!running_.load()) return;
  running_.store(false);
  if (worker_.joinable()) worker_.join();
  emit_connection(false);
}

void DeribitManager::subscribe_symbol(const std::string& symbol) {
  subs_.push_back(symbol);
}

void DeribitManager::unsubscribe_symbol(const std::string& symbol) {
  subs_.erase(std::remove(subs_.begin(), subs_.end(), symbol), subs_.end());
}

void DeribitManager::run_mock() {
  using namespace std::chrono_literals;
  while (running_.load()) {
    for (const auto& s : subs_) {
      // Emit a fake orderbook snapshot message representative of Deribit
      std::string msg = std::string("{\"channel\":\"book.") + s + "\",\"data\":{}}";
      emit_message(msg);
    }
    std::this_thread::sleep_for(50ms);
  }
}


