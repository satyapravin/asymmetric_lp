#include <iostream>
#include <cstdlib>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include "../utils/config/config.hpp"
#include "zmq_oms.hpp"
#include "market_making_strategy.hpp"
#include "models/glft_target.hpp"

int main() {
  auto cfg = load_app_config();
  std::cout << "Starting Market Making Strategy" << std::endl;

  // Create ZMQ OMS for order management
  auto oms = std::make_shared<ZMQOMS>(cfg.ord_pub_endpoint, cfg.ord_topic_new, 
                                       cfg.ord_sub_endpoint, cfg.ord_topic_ev);
  
  // Create GLFT model
  auto glft_model = std::make_shared<GlftTarget>();
  
  // Create market making strategy
  std::string md_topic = cfg.md_topic.empty() ? 
    (std::string("md.") + cfg.exchanges_csv + "." + cfg.symbol) : cfg.md_topic;
  
  std::string pos_topic = cfg.pos_topic.empty() ? 
    (std::string("pos.GRVT.") + cfg.symbol) : cfg.pos_topic;
  
  auto strategy = std::make_unique<MarketMakingStrategy>(
    cfg.symbol, cfg.md_sub_endpoint, md_topic, cfg.pos_sub_endpoint, pos_topic, oms, glft_model);
  
  // Configure strategy
  strategy->set_min_spread_bps(5.0);  // 5 bps minimum spread
  strategy->set_max_position_size(100.0);
  strategy->set_quote_size(1.0);
  
  // Set up order event handling
  oms->set_event_callback([&strategy](const std::string& cl_ord_id,
                                     const std::string& exch,
                                     const std::string& symbol,
                                     uint32_t event_type,
                                     double fill_qty,
                                     double fill_price,
                                     const std::string& text) {
    // Forward order events to strategy
    strategy->on_order_event(cl_ord_id, exch, symbol, event_type, fill_qty, fill_price, text);
  });
  
  // Start strategy
  strategy->start();
  
  // Poll for inventory updates from Python LP
  ZmqSubscriber inventory_sub(cfg.zmq_endpoint, cfg.zmq_topic);
  
  std::cout << "Market making strategy running. Press Ctrl+C to stop." << std::endl;
  
  while (true) {
    // Poll for inventory updates
    auto inv_msg = inventory_sub.receive();
    if (inv_msg) {
      auto delta = ZmqSubscriber::parse_minimal_delta(*inv_msg);
      if (delta) {
        strategy->set_inventory_delta(delta->delta_units);
        std::cout << "[INVENTORY] Delta updated: " << delta->delta_units << std::endl;
      }
    }
    
    // Poll for order events
    oms->poll_events();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  strategy->stop();
  return 0;
}


