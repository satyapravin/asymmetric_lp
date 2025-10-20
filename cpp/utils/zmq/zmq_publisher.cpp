#include "zmq_publisher.hpp"
#include <cstring>
#include <iostream>

ZmqPublisher::ZmqPublisher(const std::string& bind_endpoint, int hwm)
  : ctx_(zmq_ctx_new()), pub_(nullptr), endpoint_(bind_endpoint), hwm_(hwm), bound_(false) {
  pub_ = zmq_socket(ctx_, ZMQ_PUB);
  if (pub_) {
    zmq_setsockopt(pub_, ZMQ_SNDHWM, &hwm_, sizeof(hwm_));
    // Auto-bind to preserve legacy behavior
    bind();
  }
}

ZmqPublisher::~ZmqPublisher() {
  if (pub_) {
    zmq_close(pub_);
    pub_ = nullptr;
  }
  if (ctx_) {
    zmq_ctx_term(ctx_);
    ctx_ = nullptr;
  }
}

bool ZmqPublisher::bind() {
  if (!pub_) return false;
  if (bound_) return true;
  if (zmq_bind(pub_, endpoint_.c_str()) != 0) {
    std::cerr << "[ZmqPublisher] Failed to bind to: " << endpoint_ << std::endl;
    return false;
  }
  std::cout << "[ZmqPublisher] Successfully bound to: " << endpoint_ << std::endl;
  bound_ = true;
  return true;
}

bool ZmqPublisher::send(const std::string& topic, const void* data, size_t size, int flags) {
  if (!pub_ || !bound_) return false;
  zmq_msg_t msg_topic;
  zmq_msg_init_size(&msg_topic, topic.size());
  std::memcpy(zmq_msg_data(&msg_topic), topic.data(), topic.size());
  if (zmq_msg_send(&msg_topic, pub_, ZMQ_SNDMORE) == -1) {
    zmq_msg_close(&msg_topic);
    return false;
  }
  zmq_msg_close(&msg_topic);

  zmq_msg_t msg_payload;
  zmq_msg_init_size(&msg_payload, size);
  std::memcpy(zmq_msg_data(&msg_payload), data, size);
  bool ok = zmq_msg_send(&msg_payload, pub_, flags) != -1;
  zmq_msg_close(&msg_payload);
  return ok;
}

bool ZmqPublisher::send_string(const std::string& topic, const std::string& payload, int flags) {
  std::cout << "[ZmqPublisher] Publishing to topic: " << topic << " payload size: " << payload.size() << " bytes" << std::endl;
  bool result = send(topic, payload.data(), payload.size(), flags);
  if (result) {
    std::cout << "[ZmqPublisher] Successfully published message" << std::endl;
  } else {
    std::cout << "[ZmqPublisher] Failed to publish message" << std::endl;
  }
  return result;
}
