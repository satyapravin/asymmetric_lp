#include <iostream>
#include <cstdlib>
#include <string>
#include "zmq_subscriber.hpp"
#include "glft_target.hpp"
#include "grvt_client_stub.hpp"

int main() {
  const char* zmq_endpoint = std::getenv("ZMQ_SUBSCRIBER_ENDPOINT");
  if (!zmq_endpoint) zmq_endpoint = "tcp://127.0.0.1:5555";
  std::cout << "Connecting ZMQ SUB to " << zmq_endpoint << std::endl;

  ZmqSubscriber sub(zmq_endpoint, "inventory_update");
  GlftTarget glft;
  GrvtClientStub grvt;

  while (true) {
    auto msg = sub.receive();
    if (!msg.has_value()) continue;
    const auto& payload = msg.value();
    // Expect minimal JSON: { asset_token, asset_symbol, delta_units }
    auto parsed = ZmqSubscriber::parse_minimal_delta(payload);
    if (!parsed.has_value()) continue;

    const auto& d = parsed.value();
    double delta = d.delta_units;
    // Target inventory is negative of delta
    double target = glft.compute_target(-delta);
    // Issue perp adjustment to move current toward target
    double order_qty = target - (-delta); // target minus current (which is -delta)
    if (std::abs(order_qty) < 1e-9) continue;
    grvt.submit_perp_order(d.asset_symbol, order_qty);
  }
  return 0;
}


