#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include "models/glft_target.hpp"
#include "../utils/oms/oms.hpp"
#include "../utils/mds/market_data.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"

class GLFTMarketMaker {
public:
  GLFTMarketMaker(OMS& oms,
                  MarketDataBus& md_bus,
                  GlftTarget& glft,
                  const std::string& zmq_endpoint,
                  const std::string& zmq_topic,
                  const std::string& exch,
                  const std::string& symbol,
                  double min_order_qty,
                  double max_order_qty)
      : oms_(oms), md_bus_(md_bus), glft_(glft), exch_(exch), symbol_(symbol), sub_(zmq_endpoint, zmq_topic), min_order_qty_(min_order_qty), max_order_qty_(max_order_qty) {
    oms_.on_event = [this](const OrderEvent& ev) { this->on_order_event(ev); };
    md_bus_.on_snapshot = [this](const OrderBookSnapshot& ob) { this->on_snapshot(ob); };
  }

  void start() {
    md_bus_.subscribe(exch_, symbol_);
  }

  void poll_zmq_once() {
    auto m = sub_.receive();
    if (!m) return;
    auto d = ZmqSubscriber::parse_minimal_delta(*m);
    if (!d) return;
    double delta = d->delta_units;
    current_delta_.store(delta);
    // Compute target inventory vs perp
    double target = glft_.compute_target(-delta);
    double current = -delta; // published delta implies current perp exposure
    double adj = target - current;
    if (std::fabs(adj) < 1e-8) return;
    // sizing clamps
    double qty = std::fabs(adj);
    if (min_order_qty_ > 0.0 && qty < min_order_qty_) return;
    if (max_order_qty_ > 0.0 && qty > max_order_qty_) qty = max_order_qty_;
    // Create a simple market order toward target; size clipped
    Order o;
    o.cl_ord_id = next_cl_id();
    o.exch = exch_;
    o.symbol = symbol_;
    o.is_market = true;
    o.side = adj > 0 ? Side::Buy : Side::Sell;
    o.qty = qty;
    o.price = 0.0;
    oms_.send(o);
  }

private:
  void on_order_event(const OrderEvent& ev) {
    // For now, ignore
    (void)ev;
  }

  void on_snapshot(const OrderBookSnapshot& ob) {
    if (ob.exch != exch_ || ob.symbol != symbol_) return;
    last_bid_ = ob.bid_px;
    last_ask_ = ob.ask_px;
  }

  std::string next_cl_id() {
    auto n = ++seq_;
    return symbol_ + "-" + std::to_string(n);
  }

  OMS& oms_;
  MarketDataBus& md_bus_;
  GlftTarget& glft_;
  std::string exch_;
  std::string symbol_;
  ZmqSubscriber sub_;
  std::atomic<double> current_delta_{0.0};
  std::optional<double> last_bid_;
  std::optional<double> last_ask_;
  std::atomic<uint64_t> seq_{0};
  double min_order_qty_{0.0};
  double max_order_qty_{0.0};
};


