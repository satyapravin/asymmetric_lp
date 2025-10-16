#include "exchange_manager.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>

ExchangeManager::ExchangeManager(const std::string& exchange_name, 
                               const std::string& websocket_url)
    : exchange_name_(exchange_name), websocket_url_(websocket_url) {
  std::cout << "[EXCHANGE_MANAGER] Created for " << exchange_name_ 
            << " at " << websocket_url_ << std::endl;
}

ExchangeManager::~ExchangeManager() {
  stop();
}

bool ExchangeManager::start() {
  if (running_.load()) return true;
  
  running_.store(true);
  connected_.store(true);
  
  // Start mock websocket simulation
  mock_thread_ = std::thread([this]() {
    simulate_websocket_connection();
  });
  
  std::cout << "[EXCHANGE_MANAGER] Started mock connection for " << exchange_name_ << std::endl;
  return true;
}

void ExchangeManager::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  connected_.store(false);
  
  if (mock_thread_.joinable()) {
    mock_thread_.join();
  }
  
  std::cout << "[EXCHANGE_MANAGER] Stopped " << exchange_name_ << std::endl;
}

void ExchangeManager::subscribe_symbol(const std::string& symbol) {
  subscribed_symbols_.push_back(symbol);
  std::cout << "[EXCHANGE_MANAGER] Subscribed to " << symbol << " on " << exchange_name_ << std::endl;
}

void ExchangeManager::unsubscribe_symbol(const std::string& symbol) {
  subscribed_symbols_.erase(
    std::remove(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol),
    subscribed_symbols_.end()
  );
  std::cout << "[EXCHANGE_MANAGER] Unsubscribed from " << symbol << " on " << exchange_name_ << std::endl;
}

void ExchangeManager::simulate_websocket_connection() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> price_dist(2000.0, 3000.0);
  std::uniform_real_distribution<> qty_dist(0.1, 10.0);
  
  while (running_.load()) {
    if (connected_.load() && !subscribed_symbols_.empty()) {
      generate_mock_market_data();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz
  }
}

void ExchangeManager::generate_mock_market_data() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<> price_dist(2000.0, 3000.0);
  static std::uniform_real_distribution<> qty_dist(0.1, 10.0);
  
  for (const auto& symbol : subscribed_symbols_) {
    double price = price_dist(gen);
    double qty = qty_dist(gen);
    
    // Generate mock orderbook data
    std::ostringstream json;
    json << "{"
         << "\"symbol\":\"" << symbol << "\","
         << "\"exchange\":\"" << exchange_name_ << "\","
         << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count() << ","
         << "\"bids\":[{\"price\":" << price << ",\"qty\":" << qty << "}],"
         << "\"asks\":[{\"price\":" << (price + 1.0) << ",\"qty\":" << qty << "}]"
         << "}";
    
    if (message_callback_) {
      message_callback_(json.str());
    }
  }
}

void ExchangeManager::handle_message(const std::string& message) {
  if (message_callback_) {
    message_callback_(message);
  }
}

void ExchangeManager::handle_connection(bool connected) {
  connected_.store(connected);
  if (connection_callback_) {
    connection_callback_(connected);
  }
}