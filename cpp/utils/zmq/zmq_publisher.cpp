#include "zmq_publisher.hpp"
#include <zmq.h>
#include <cstring>

ZmqPublisher::ZmqPublisher(const std::string& bind_endpoint) {
  ctx_ = zmq_ctx_new();
  pub_ = zmq_socket(ctx_, ZMQ_PUB);
  zmq_bind(pub_, bind_endpoint.c_str());
}

ZmqPublisher::~ZmqPublisher() {
  if (pub_) zmq_close(pub_);
  if (ctx_) zmq_ctx_term(ctx_);
}

bool ZmqPublisher::publish(const std::string& topic, const std::string& payload) {
  zmq_msg_t t;
  zmq_msg_init_size(&t, topic.size());
  std::memcpy(zmq_msg_data(&t), topic.data(), topic.size());
  if (zmq_msg_send(&t, pub_, ZMQ_SNDMORE) == -1) {
    zmq_msg_close(&t);
    return false;
  }
  zmq_msg_close(&t);

  zmq_msg_t p;
  zmq_msg_init_size(&p, payload.size());
  std::memcpy(zmq_msg_data(&p), payload.data(), payload.size());
  if (zmq_msg_send(&p, pub_, 0) == -1) {
    zmq_msg_close(&p);
    return false;
  }
  zmq_msg_close(&p);
  return true;
}
