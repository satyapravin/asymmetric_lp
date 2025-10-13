#include "position_feed.hpp"
#include <iostream>
#include <random>
#include <chrono>

MockPositionFeed::MockPositionFeed() = default;

MockPositionFeed::~MockPositionFeed() {
  disconnect();
}

bool MockPositionFeed::connect(const std::string& account) {
  if (connected_.load()) return true;
  
  account_ = account;
  running_.store(true);
  generator_thread_ = std::thread([this]() { this->run_position_generator(); });
  connected_.store(true);
  
  std::cout << "[POSITION_FEED] Connected to account: " << account << std::endl;
  return true;
}

void MockPositionFeed::disconnect() {
  if (!connected_.load()) return;
  
  running_.store(false);
  if (generator_thread_.joinable()) {
    generator_thread_.join();
  }
  connected_.store(false);
  
  std::cout << "[POSITION_FEED] Disconnected from account: " << account_ << std::endl;
}

bool MockPositionFeed::is_connected() const {
  return connected_.load();
}

void MockPositionFeed::run_position_generator() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> price_dist(1900.0, 2100.0);
  std::uniform_real_distribution<> qty_dist(-5.0, 5.0);
  
  double base_price = 2000.0;
  double qty = 0.0;
  double avg_price = base_price;
  
  while (running_.load()) {
    // Simulate position changes
    double price_change = price_dist(gen) - base_price;
    double qty_change = qty_dist(gen);
    
    // Update position
    if (qty != 0) {
      // Update average price based on new trade
      avg_price = (avg_price * std::abs(qty) + base_price * std::abs(qty_change)) / 
                  (std::abs(qty) + std::abs(qty_change));
    } else {
      avg_price = base_price;
    }
    
    qty += qty_change;
    
    // Publish position update
    if (on_position_update) {
      on_position_update("ETHUSDC-PERP", "GRVT", qty, avg_price);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1Hz updates
  }
}
