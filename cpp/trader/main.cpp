#include <iostream>
#include <cstdlib>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include "../utils/config/config.hpp"
#include "../utils/handlers/message_handler_manager.hpp"
#include "zmq_oms.hpp"
#include "market_making_strategy.hpp"
#include "models/glft_target.hpp"

int main() {
  auto cfg = load_app_config();
  std::cout << "Starting Market Making Strategy" << std::endl;
  std::cout << "[DEBUG] Loaded " << cfg.message_handlers.size() << " message handlers from config" << std::endl;
  
  for (const auto& handler : cfg.message_handlers) {
    std::cout << "[DEBUG] Handler: " << handler.name << " endpoint: " << handler.endpoint 
              << " topic: " << handler.topic << " enabled: " << handler.enabled << std::endl;
  }

  // Create ZMQ OMS for order management
  auto oms = std::make_shared<ZMQOMS>(cfg.ord_pub_endpoint, cfg.ord_topic_new, 
                                       cfg.ord_sub_endpoint, cfg.ord_topic_ev);
  
  // Create GLFT model
  auto glft_model = std::make_shared<GlftTarget>();
  
  // Create market making strategy (no legacy feed gating; handlers drive feeds)
  auto strategy = std::make_unique<MarketMakingStrategy>(
    cfg.symbol, glft_model, cfg.md_pub_endpoint, "market_data", 
    cfg.pos_pub_endpoint, "positions", "", "");
  
  // Configure strategy
  strategy->set_min_spread_bps(5.0);  // 5 bps minimum spread
  strategy->set_max_position_size(100.0);
  strategy->set_quote_size(1.0);
  
  // Set up order event handling
  strategy->set_order_event_callback([](const OrderEvent& event) {
    std::cout << "[ORDER_EVENT] " << event.cl_ord_id << " " << event.exch << " " << event.symbol 
              << " " << to_string(event.type) << " qty=" << event.fill_qty << " price=" << event.fill_price 
              << " " << event.text << std::endl;
  });
  
  // Start strategy
  strategy->start();
  
  // Create message handler manager
  auto handler_manager = std::make_unique<MessageHandlerManager>();
  
  // Set up message handler callback
  handler_manager->set_data_callback([&strategy](const std::string& handler_name, const std::string& data) {
    strategy->on_message(handler_name, data);
  });
  
  // Load message handlers from config
  handler_manager->load_from_config(cfg.message_handlers);
  
  // Start all message handlers
  handler_manager->start_all();
  
  std::cout << "Market making strategy running. Press Ctrl+C to stop." << std::endl;
  std::cout << "Handlers drive feeds; legacy enable flags removed." << std::endl;
  std::cout << "Message handlers: " << handler_manager->get_handler_count() << std::endl;
  
  // Optional: legacy direct inventory SUB is disabled in favor of handler config
  std::unique_ptr<ZmqSubscriber> inventory_sub; // unused unless a handler invokes it
  
  while (true) {
    // Legacy inventory path disabled (use message handlers instead)
    if (false && inventory_sub) {
      auto inv_msg = inventory_sub->receive();
      if (inv_msg) {
        auto delta = ZmqSubscriber::parse_minimal_delta(*inv_msg);
        if (delta) {
          strategy->set_inventory_delta(delta->delta_units);
          std::cout << "[INVENTORY] Delta updated: " << delta->delta_units << std::endl;
        }
      }
    }
    // OMS events
    oms->poll_events();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  strategy->stop();
  return 0;
}


