#include "zmq_oms.hpp"
#include <iostream>

ZMQOMS::ZMQOMS(const std::string& order_pub_endpoint,
               const std::string& order_topic,
               const std::string& event_sub_endpoint,
               const std::string& event_topic)
    : order_topic_(order_topic), event_topic_(event_topic) {
  order_publisher_ = std::make_unique<ZmqPublisher>(order_pub_endpoint);
  event_subscriber_ = std::make_unique<ZmqSubscriber>(event_sub_endpoint, event_topic);
}

ZMQOMS::~ZMQOMS() = default;

bool ZMQOMS::send_order(const std::string& cl_ord_id,
                       const std::string& exch,
                       const std::string& symbol,
                       uint32_t side,
                       uint32_t is_market,
                       double qty,
                       double price) {
  char buffer[OrderBinaryHelper::ORDER_SIZE];
  OrderBinaryHelper::serialize_order(cl_ord_id, exch, symbol, side, is_market, qty, price, buffer);
  
  return order_publisher_->publish(order_topic_, std::string(buffer, OrderBinaryHelper::ORDER_SIZE));
}

bool ZMQOMS::cancel_order(const std::string& cl_ord_id,
                          const std::string& exch) {
  // For now, just log the cancel request
  std::cout << "[ZMQOMS] Cancel order: " << cl_ord_id << " on " << exch << std::endl;
  return true;
}

void ZMQOMS::poll_events() {
  auto msg = event_subscriber_->receive();
  if (msg) {
    process_event_message(*msg);
  }
}

void ZMQOMS::process_event_message(const std::string& msg) {
  if (msg.size() == OrderBinaryHelper::ORDER_EVENT_SIZE) {
    std::string cl_ord_id, exch, symbol, text;
    uint32_t event_type;
    double fill_qty, fill_price;
    
    if (OrderBinaryHelper::deserialize_order_event(msg.data(), cl_ord_id, exch, symbol, 
                                                 event_type, fill_qty, fill_price, text)) {
      if (event_callback_) {
        event_callback_(cl_ord_id, exch, symbol, event_type, fill_qty, fill_price, text);
      }
    }
  }
}
