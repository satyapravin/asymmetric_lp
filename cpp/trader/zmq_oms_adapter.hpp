#pragma once
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include "../utils/oms/order_binary.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/zmq/zmq_subscriber.hpp"

// Order Management System that publishes orders via ZMQ and receives events
class ZmqOMSAdapter {
public:
  using OrderEventCallback = std::function<void(const std::string& cl_ord_id,
                                               const std::string& exch,
                                               const std::string& symbol,
                                               uint32_t event_type,
                                               double fill_qty,
                                               double fill_price,
                                               const std::string& text)>;
  
  ZmqOMSAdapter(const std::string& order_pub_endpoint,
         const std::string& order_topic,
         const std::string& event_sub_endpoint,
         const std::string& event_topic);
  
  ~ZmqOMSAdapter();
  
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
  
  // Modify order (replace with new price/quantity)
  bool modify_order(const std::string& cl_ord_id,
                    const std::string& exch,
                    double new_price,
                    double new_qty);
  
  // Poll for events (non-blocking)
  void poll_events();

private:
  void process_event_message(const std::string& msg);
  
  std::unique_ptr<ZmqPublisher> order_publisher_;
  std::unique_ptr<ZmqSubscriber> event_subscriber_;
  std::string order_topic_;
  std::string event_topic_;
  std::string cancel_topic_;  // Topic for cancel requests
  std::string modify_topic_;  // Topic for modify requests
  OrderEventCallback event_callback_;
  std::atomic<uint32_t> sequence_{0};
};
