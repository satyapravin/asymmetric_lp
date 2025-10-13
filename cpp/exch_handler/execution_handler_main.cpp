#include <iostream>
#include <string>
#include <optional>
#include <chrono>
#include "../utils/config/config.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/oms/order_binary.hpp"

// Very simple order schema: {cl_id, exch, symbol, side, qty}
static std::optional<std::string> parse_field(const std::string& json, const std::string& key) {
  auto kpos = json.find("\"" + key + "\"");
  if (kpos == std::string::npos) return std::nullopt;
  auto cpos = json.find(':', kpos);
  if (cpos == std::string::npos) return std::nullopt;
  auto start = json.find_first_not_of(" \"", cpos + 1);
  auto end = json.find_first_of(",}\n\r\t\"", start);
  if (start == std::string::npos) return std::nullopt;
  if (end == std::string::npos) end = json.size();
  return json.substr(start, end - start);
}

int main() {
  auto cfg = load_app_config();
  // Orders subscriber (router publishes orders to topic configured)
  std::string ord_sub_endpoint = cfg.ord_pub_endpoint; // MM publishes orders here; exec SUBs
  ZmqSubscriber sub(ord_sub_endpoint, cfg.ord_topic_new);
  // Status publisher (this process publishes to events topic)
  std::string ev_pub_endpoint = cfg.ord_sub_endpoint; // exec PUBs events here; MM SUBs
  ZmqPublisher pub(ev_pub_endpoint);
  std::cout << "Execution handler listening on " << ord_sub_endpoint << ", publishing on " << ev_pub_endpoint << std::endl;

  while (true) {
    auto msg = sub.receive();
    if (!msg) continue;
    
    // Try to parse as binary order first
    if (msg->size() == OrderBinaryHelper::ORDER_SIZE) {
      std::string cl_ord_id, exch, symbol;
      uint32_t side, is_market;
      double qty, price;
      
      if (OrderBinaryHelper::deserialize_order(msg->data(), cl_ord_id, exch, symbol, side, is_market, qty, price)) {
        std::cout << "[EXEC] Binary order: " << cl_ord_id << " " << exch << " " << symbol 
                  << " " << (side == 0 ? "BUY" : "SELL") << " " << qty << " @ " << price << std::endl;
        
        // Send Ack
        char ack_buffer[OrderBinaryHelper::ORDER_EVENT_SIZE];
        OrderBinaryHelper::serialize_order_event(cl_ord_id, exch, symbol, 0, 0.0, 0.0, "ack", ack_buffer);
        pub.publish(cfg.ord_topic_ev, std::string(ack_buffer, OrderBinaryHelper::ORDER_EVENT_SIZE));
        
        // Send Fill (immediate for now)
        char fill_buffer[OrderBinaryHelper::ORDER_EVENT_SIZE];
        OrderBinaryHelper::serialize_order_event(cl_ord_id, exch, symbol, 1, qty, price, "filled", fill_buffer);
        pub.publish(cfg.ord_topic_ev, std::string(fill_buffer, OrderBinaryHelper::ORDER_EVENT_SIZE));
        
        continue;
      }
    }
    
    // Fallback to JSON parsing for backward compatibility
    auto cl = parse_field(*msg, "cl_id");
    auto exch = parse_field(*msg, "exch");
    auto symbol = parse_field(*msg, "symbol");
    auto side = parse_field(*msg, "side");
    auto qty = parse_field(*msg, "qty");
    if (!cl || !exch || !symbol || !side || !qty) continue;

    // Ack
    std::string ack = std::string("{\"cl_id\":\"") + *cl + "\",\"exch\":\"" + *exch + "\",\"symbol\":\"" + *symbol + "\",\"type\":\"Ack\"}";
    pub.publish(cfg.ord_topic_ev, ack);
    // Fill immediately for now
    std::string fill = std::string("{\"cl_id\":\"") + *cl + "\",\"exch\":\"" + *exch + "\",\"symbol\":\"" + *symbol + "\",\"type\":\"Fill\",\"fill_qty\":" + *qty + "}";
    pub.publish(cfg.ord_topic_ev, fill);
  }
  return 0;
}
