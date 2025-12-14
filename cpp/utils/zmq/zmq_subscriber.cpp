#include "zmq_subscriber.hpp"
#include "../logging/log_helper.hpp"
#include <zmq.h>
#include <cstring>

ZmqSubscriber::ZmqSubscriber(const std::string& endpoint, const std::string& topic)
    : topic_(topic) {
  ctx_ = zmq_ctx_new();
  sub_ = zmq_socket(ctx_, ZMQ_SUB);
  zmq_setsockopt(sub_, ZMQ_SUBSCRIBE, topic_.data(), topic_.size());
  if (zmq_connect(sub_, endpoint.c_str()) != 0) {
    LOG_ERROR_COMP("ZmqSubscriber", "ZMQ connect failed to " + endpoint + ": " + zmq_strerror(zmq_errno()));
  } else {
    LOG_INFO_COMP("ZmqSubscriber", "Successfully connected to: " + endpoint + " topic: " + topic);
  }
}

ZmqSubscriber::~ZmqSubscriber() {
  if (sub_ != nullptr) {
    zmq_close(sub_);
    sub_ = nullptr;
  }
  if (ctx_ != nullptr) {
    zmq_ctx_term(ctx_);
    ctx_ = nullptr;
  }
}

std::optional<std::string> ZmqSubscriber::receive() {
  zmq_msg_t topic;
  zmq_msg_init(&topic);
  if (zmq_msg_recv(&topic, sub_, 0) == -1) {
    zmq_msg_close(&topic);
    return std::nullopt;
  }
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  if (zmq_msg_recv(&msg, sub_, 0) == -1) {
    zmq_msg_close(&topic);
    zmq_msg_close(&msg);
    return std::nullopt;
  }
  std::string payload(static_cast<char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
  zmq_msg_close(&topic);
  zmq_msg_close(&msg);
  // Topic and payload are separate multipart messages already; return payload as-is.
  return payload;
}

std::optional<std::string> ZmqSubscriber::receive_blocking(int timeout_ms) {
  // Set receive timeout
  zmq_setsockopt(sub_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
  
  zmq_msg_t topic;
  zmq_msg_init(&topic);
  int rc = zmq_msg_recv(&topic, sub_, 0);
  if (rc == -1) {
    zmq_msg_close(&topic);
    return std::nullopt;
  }
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  rc = zmq_msg_recv(&msg, sub_, 0);
  if (rc == -1) {
    zmq_msg_close(&topic);
    zmq_msg_close(&msg);
    return std::nullopt;
  }
  std::string payload(static_cast<char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
  std::string topic_str(static_cast<char*>(zmq_msg_data(&topic)), zmq_msg_size(&topic));
  LOG_INFO_COMP("ZmqSubscriber", "Received message - topic: " + topic_str + " payload size: " + std::to_string(payload.size()) + " bytes");
  zmq_msg_close(&topic);
  zmq_msg_close(&msg);
  return payload;
}


