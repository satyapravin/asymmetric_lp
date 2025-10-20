#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "../proto/market_data.pb.h"

class IExchangeMD {
public:
  virtual ~IExchangeMD() = default;
  virtual void subscribe(const std::string& symbol) = 0;
  // Bus connects this to callback
  std::function<void(const proto::OrderBookSnapshot&)> on_snapshot;
};

class MarketDataBus {
public:
  using SnapshotCallback = std::function<void(const proto::OrderBookSnapshot&)>;

  void register_exchange(const std::string& exch, std::shared_ptr<IExchangeMD> handler) {
    handlers_[exch] = std::move(handler);
    handlers_[exch]->on_snapshot = [this](const proto::OrderBookSnapshot& ob) {
      if (this->on_snapshot) this->on_snapshot(ob);
    };
  }

  void subscribe(const std::string& exch, const std::string& symbol) {
    auto it = handlers_.find(exch);
    if (it == handlers_.end()) return;
    it->second->subscribe(symbol);
  }

  SnapshotCallback on_snapshot; // set by strategy

private:
  std::unordered_map<std::string, std::shared_ptr<IExchangeMD>> handlers_;
};


