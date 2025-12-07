#include "zmq_oms_adapter.hpp"
#include "../utils/logging/log_helper.hpp"
#ifdef PROTO_ENABLED
#include "../proto/order.pb.h"
#endif

ZmqOMSAdapter::ZmqOMSAdapter(const std::string& order_pub_endpoint,
               const std::string& order_topic,
               const std::string& event_sub_endpoint,
               const std::string& event_topic)
    : order_topic_(order_topic), event_topic_(event_topic),
      cancel_topic_("cancel_orders"), modify_topic_("modify_orders") {
  order_publisher_ = std::make_unique<ZmqPublisher>(order_pub_endpoint);
  event_subscriber_ = std::make_unique<ZmqSubscriber>(event_sub_endpoint, event_topic);
  LOG_INFO_COMP("ZmqOMSAdapter", "Created OMS adapter - subscribing to: " + event_sub_endpoint + 
                " topic: " + event_topic);
}

ZmqOMSAdapter::~ZmqOMSAdapter() = default;

bool ZmqOMSAdapter::send_order(const std::string& cl_ord_id,
                       const std::string& exch,
                       const std::string& symbol,
                       uint32_t side,
                       uint32_t is_market,
                       double qty,
                       double price) {
#ifdef PROTO_ENABLED
  proto::OrderRequest req;
  req.set_cl_ord_id(cl_ord_id);
  req.set_exch(exch);
  req.set_symbol(symbol);
  req.set_side(side == 0 ? proto::BUY : proto::SELL);
  req.set_type(is_market ? proto::MARKET : proto::LIMIT);
  req.set_qty(qty);
  req.set_price(price);
  std::string payload;
  req.SerializeToString(&payload);
  return order_publisher_->publish(order_topic_, payload);
#else
  char buffer[OrderBinaryHelper::ORDER_SIZE];
  OrderBinaryHelper::serialize_order(cl_ord_id, exch, symbol, side, is_market, qty, price, buffer);
  return order_publisher_->publish(order_topic_, std::string(buffer, OrderBinaryHelper::ORDER_SIZE));
#endif
}

bool ZmqOMSAdapter::cancel_order(const std::string& cl_ord_id,
                          const std::string& exch) {
#ifdef PROTO_ENABLED
  // Create cancel request message
  proto::OrderRequest cancel_req;
  cancel_req.set_cl_ord_id(cl_ord_id);
  cancel_req.set_exch(exch);
  // Use a special symbol to indicate cancel request
  cancel_req.set_symbol("__CANCEL__");
  cancel_req.set_side(proto::BUY); // Dummy value
  cancel_req.set_type(proto::LIMIT); // Dummy value
  cancel_req.set_qty(0.0); // Zero quantity indicates cancel
  cancel_req.set_price(0.0);
  cancel_req.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  
  std::string payload;
  if (cancel_req.SerializeToString(&payload)) {
    // Publish to order_topic_ instead of cancel_topic_ so TradingEngine can process it
    // TradingEngineLib::handle_order_request() checks for "__CANCEL__" symbol
    bool success = order_publisher_->publish(order_topic_, payload);
    if (success) {
      LOG_DEBUG_COMP("ZmqOMSAdapter", "Cancel order request sent: " + cl_ord_id + " on " + exch);
    } else {
      LOG_ERROR_COMP("ZmqOMSAdapter", "Failed to publish cancel order: " + cl_ord_id);
    }
    return success;
  } else {
    LOG_ERROR_COMP("ZmqOMSAdapter", "Failed to serialize cancel order request");
    return false;
  }
#else
  // Binary format - create cancel message
  // For binary format, we'd need to extend OrderBinaryHelper
  LOG_WARN_COMP("ZmqOMSAdapter", "Cancel order not fully implemented for binary format");
    return false;
#endif
}

bool ZmqOMSAdapter::modify_order(const std::string& cl_ord_id,
                                  const std::string& exch,
                                  double new_price,
                                  double new_qty) {
#ifdef PROTO_ENABLED
  // Create modify request message
  proto::OrderRequest modify_req;
  modify_req.set_cl_ord_id(cl_ord_id);
  modify_req.set_exch(exch);
  // Use a special symbol to indicate modify request
  modify_req.set_symbol("__MODIFY__");
  modify_req.set_side(proto::BUY); // Dummy value
  modify_req.set_type(proto::LIMIT);
  modify_req.set_qty(new_qty);
  modify_req.set_price(new_price);
  modify_req.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  
  std::string payload;
  if (modify_req.SerializeToString(&payload)) {
    // Publish to order_topic_ instead of modify_topic_ so TradingEngine can process it
    // TradingEngineLib::handle_order_request() checks for "__MODIFY__" symbol
    bool success = order_publisher_->publish(order_topic_, payload);
    if (success) {
      LOG_DEBUG_COMP("ZmqOMSAdapter", "Modify order request sent: " + cl_ord_id + 
                    " new_price=" + std::to_string(new_price) + 
                    " new_qty=" + std::to_string(new_qty));
    } else {
      LOG_ERROR_COMP("ZmqOMSAdapter", "Failed to publish modify order: " + cl_ord_id);
    }
    return success;
  } else {
    LOG_ERROR_COMP("ZmqOMSAdapter", "Failed to serialize modify order request");
    return false;
  }
#else
  LOG_WARN_COMP("ZmqOMSAdapter", "Modify order not fully implemented for binary format");
  return false;
#endif
}

void ZmqOMSAdapter::poll_events() {
  auto msg = event_subscriber_->receive_blocking(100); // 100ms timeout
  if (msg) {
    LOG_INFO_COMP("ZmqOMSAdapter", "Received message of size: " + std::to_string(msg->size()) + " bytes");
    process_event_message(*msg);
  }
}

void ZmqOMSAdapter::process_event_message(const std::string& msg) {
#ifdef PROTO_ENABLED
  // Try to parse as protobuf first
  proto::OrderEvent order_event;
  if (order_event.ParseFromString(msg)) {
    LOG_DEBUG_COMP("ZmqOMSAdapter", "Successfully parsed protobuf order event: " + 
                  order_event.cl_ord_id() + " " + order_event.symbol());
    if (event_callback_) {
      // Convert protobuf to legacy callback format for compatibility
      std::string cl_ord_id = order_event.cl_ord_id();
      std::string exch = order_event.exch();
      std::string symbol = order_event.symbol();
      uint32_t event_type = static_cast<uint32_t>(order_event.event_type());
      double fill_qty = order_event.fill_qty();
      double fill_price = order_event.fill_price();
      std::string text = order_event.text();
      
      LOG_DEBUG_COMP("ZmqOMSAdapter", "Calling event callback for: " + cl_ord_id);
      event_callback_(cl_ord_id, exch, symbol, event_type, fill_qty, fill_price, text);
    } else {
      LOG_WARN_COMP("ZmqOMSAdapter", "No event callback set!");
    }
    return;
  } else {
    LOG_WARN_COMP("ZmqOMSAdapter", "Failed to parse as protobuf, trying binary format");
  }
#endif
  
  // Fallback to binary format
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
