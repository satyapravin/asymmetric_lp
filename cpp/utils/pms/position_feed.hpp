#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

// Exchange-specific position feed interface
class IExchangePositionFeed {
public:
  virtual ~IExchangePositionFeed() = default;
  virtual bool connect(const std::string& account) = 0;
  virtual void disconnect() = 0;
  virtual bool is_connected() const = 0;
  
  // Position update callback
  std::function<void(const std::string& symbol,
                    const std::string& exch,
                    double qty,
                    double avg_price)> on_position_update;
};

// Mock position feed for testing
class MockPositionFeed : public IExchangePositionFeed {
public:
  MockPositionFeed();
  ~MockPositionFeed();
  
  bool connect(const std::string& account) override;
  void disconnect() override;
  bool is_connected() const override;

private:
  void run_position_generator();
  
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};
  std::thread generator_thread_;
  std::string account_;
};
