#pragma once
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include "../utils/oms/order_binary.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"

// Order Management System that publishes orders via ZMQ and receives events
class ZMQOMS {
public:
  using OrderEventCallback = std::function<void(const std::string& cl_ord_id,
                                               const std::string& exch,
                                               const std::string& symbol,
                                               uint32_t event_type,
                                               double fill_qty,
                                               double fill_price,
                                               const std::string& text)>;
  
  ZMQOMS(const std::string& order_pub_endpoint,
         const std::string& order_topic,
         const std::string& event_sub_endpoint,
         const std::string& event_topic);
  
  ~ZMQOMS();
  
  void set_event_callback(OrderEventCallback callback) {
    event_callback_ = callback;
  }
  
  // Send order via ZMQ
  bool send_order(const std::string& cl_ord_id,
                  const std::string& exch,
                  const std::string& symbol,
                  uint32_t side,  // 0=Buy, 1=Sell
                  uint32_t is_market,  // 0=Limit, 1=Market
                  double qty,
                  double price);
  
  // Cancel order
  bool cancel_order(const std::string& cl_ord_id,
                    const std::string& exch);
  
  // Poll for events (non-blocking)
  void poll_events();

private:
  void process_event_message(const std::string& msg);
  
  std::unique_ptr<ZmqPublisher> order_publisher_;
  std::unique_ptr<ZmqSubscriber> event_subscriber_;
  std::string order_topic_;
  std::string event_topic_;
  OrderEventCallback event_callback_;
  std::atomic<uint32_t> sequence_{0};
};
