#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "order.hpp"

// Exchange-specific OMS interface
class IExchangeOMS {
public:
  virtual ~IExchangeOMS() = default;
  virtual void send(const Order& order) = 0;
  virtual void cancel(const std::string& cl_ord_id) = 0;
  // crude event hookup
  std::function<void(const OrderEvent&)> on_event;
};

// Router OMS: routes by order.exch to a registered handler
class OMS {
public:
  using EventCallback = std::function<void(const OrderEvent&)>;

  void register_exchange(const std::string& exch, std::shared_ptr<IExchangeOMS> handler) {
    handlers_[exch] = std::move(handler);
    // Fan-in events to OMS callback
    handlers_[exch]->on_event = [this](const OrderEvent& ev) {
      if (this->on_event) this->on_event(ev);
    };
  }

  void send(const Order& order) {
    auto it = handlers_.find(order.exch);
    if (it == handlers_.end()) {
      if (on_event) on_event(OrderEvent{order.cl_ord_id, order.exch, order.symbol, OrderEventType::Reject, 0.0, 0.0, "Unknown exchange"});
      return;
    }
    it->second->send(order);
  }

  void cancel(const std::string& exch, const std::string& cl_ord_id) {
    auto it = handlers_.find(exch);
    if (it == handlers_.end()) return;
    it->second->cancel(cl_ord_id);
  }

  EventCallback on_event; // set by strategy to receive events

private:
  std::unordered_map<std::string, std::shared_ptr<IExchangeOMS>> handlers_;
};


