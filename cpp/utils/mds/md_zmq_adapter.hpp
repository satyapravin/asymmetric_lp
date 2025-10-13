#pragma once
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include "market_data.hpp"
#include "zmq_subscriber.hpp"

class ZmqMDAdapter : public IExchangeMD {
public:
  ZmqMDAdapter(const std::string& endpoint, const std::string& topic, const std::string& exch)
      : endpoint_(endpoint), topic_(topic), exch_(exch) {
    running_.store(true);
    worker_ = std::thread([this]() { this->run(); });
  }

  ~ZmqMDAdapter() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
  }

  void subscribe(const std::string& symbol) override {
    (void)symbol;
  }

  std::function<void(const OrderBookSnapshot&)> on_snapshot;

private:
  void run() {
    ZmqSubscriber sub(endpoint_, topic_);
    while (running_.load()) {
      auto msg = sub.receive();
      if (!msg) continue;
      auto ob = parse_ob(*msg);
      if (!ob) continue;
      if (on_snapshot) on_snapshot(*ob);
    }
  }

  static std::optional<OrderBookSnapshot> parse_ob(const std::string& json) {
    auto find = [&](const std::string& key) -> std::optional<std::string> {
      auto k = json.find("\"" + key + "\"");
      if (k == std::string::npos) return std::nullopt;
      auto c = json.find(':', k);
      if (c == std::string::npos) return std::nullopt;
      auto s = json.find_first_not_of(" \"", c + 1);
      auto e = json.find_first_of(",}\n\r\t\"", s);
      if (s == std::string::npos) return std::nullopt;
      if (e == std::string::npos) e = json.size();
      return json.substr(s, e - s);
    };
    auto exch = find("exch");
    auto sym = find("symbol");
    auto bid_px = find("bid_px");
    auto bid_sz = find("bid_sz");
    auto ask_px = find("ask_px");
    auto ask_sz = find("ask_sz");
    if (!exch || !sym || !bid_px || !ask_px || !bid_sz || !ask_sz) return std::nullopt;
    OrderBookSnapshot ob;
    ob.exch = *exch;
    if (!ob.exch.empty() && ob.exch.front()=='"') ob.exch.erase(0,1);
    if (!ob.exch.empty() && ob.exch.back()=='"') ob.exch.pop_back();
    ob.symbol = *sym;
    if (!ob.symbol.empty() && ob.symbol.front()=='"') ob.symbol.erase(0,1);
    if (!ob.symbol.empty() && ob.symbol.back()=='"') ob.symbol.pop_back();
    try {
      ob.bid_px = std::stod(*bid_px);
      ob.ask_px = std::stod(*ask_px);
      ob.bid_sz = std::stod(*bid_sz);
      ob.ask_sz = std::stod(*ask_sz);
    } catch (...) { return std::nullopt; }
    return ob;
  }

  std::string endpoint_;
  std::string topic_;
  std::string exch_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};
