#include "zmq_publisher.hpp"
#include "../logging/log_helper.hpp"
#include <cstring>
#include <cerrno>

ZmqPublisher::ZmqPublisher(const std::string& bind_endpoint, int hwm, bool conflate)
  : ctx_(nullptr), pub_(nullptr), endpoint_(bind_endpoint), hwm_(hwm), bound_(false),
    conflate_(conflate), messages_sent_(0), messages_dropped_(0) {
  // Create ZMQ context first
  ctx_ = zmq_ctx_new();
  if (!ctx_) {
    throw std::runtime_error("Failed to create ZMQ context");
  }
  
  // Create socket with proper error handling
  try {
    pub_ = zmq_socket(ctx_, ZMQ_PUB);
    if (!pub_) {
      // Clean up context before throwing
      zmq_ctx_term(ctx_);
      ctx_ = nullptr;
      throw std::runtime_error("Failed to create ZMQ socket: " + std::string(zmq_strerror(zmq_errno())));
    }
  } catch (...) {
    // Ensure context is cleaned up on any exception
    if (ctx_) {
      zmq_ctx_term(ctx_);
      ctx_ = nullptr;
    }
    throw;
  }
  
  if (pub_) {
    zmq_setsockopt(pub_, ZMQ_SNDHWM, &hwm_, sizeof(hwm_));
    
    // ZMQ_CONFLATE: Keep only the latest message (fire-and-forget for latest state)
    // Perfect for market data where only current state matters
    if (conflate_) {
      int conflate_value = 1;
      if (zmq_setsockopt(pub_, ZMQ_CONFLATE, &conflate_value, sizeof(conflate_value)) == 0) {
        LOG_INFO_COMP("ZmqPublisher", "CONFLATE enabled - keeping only latest message per topic");
      } else {
        LOG_WARN_COMP("ZmqPublisher", "Failed to set ZMQ_CONFLATE option");
      }
    }
    
    // ZMQ_IMMEDIATE: Don't queue messages if no subscribers connected
    // Reduces memory usage when no consumers are present
    int immediate = 1;
    if (zmq_setsockopt(pub_, ZMQ_IMMEDIATE, &immediate, sizeof(immediate)) == 0) {
      LOG_DEBUG_COMP("ZmqPublisher", "IMMEDIATE enabled - messages dropped if no subscribers");
    }
    
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
    LOG_ERROR_COMP("ZmqPublisher", "Failed to bind to: " + endpoint_);
    return false;
  }
  LOG_INFO_COMP("ZmqPublisher", "Successfully bound to: " + endpoint_);
  bound_ = true;
  return true;
}

bool ZmqPublisher::send(const std::string& topic, const void* data, size_t size, int flags) {
  if (!pub_ || !bound_) return false;
  
  zmq_msg_t msg_topic;
  zmq_msg_init_size(&msg_topic, topic.size());
  std::memcpy(zmq_msg_data(&msg_topic), topic.data(), topic.size());
  
  // Send topic frame (with SNDMORE flag)
  if (zmq_msg_send(&msg_topic, pub_, ZMQ_SNDMORE) == -1) {
    int err = zmq_errno();
    zmq_msg_close(&msg_topic);
    if (err == EAGAIN) {
      messages_dropped_.fetch_add(1);
      LOG_WARN_COMP("ZmqPublisher", "Send buffer full - topic frame dropped for topic: " + topic);
    }
    return false;
  }
  zmq_msg_close(&msg_topic);

  // Send payload frame
  zmq_msg_t msg_payload;
  zmq_msg_init_size(&msg_payload, size);
  std::memcpy(zmq_msg_data(&msg_payload), data, size);
  
  int rc = zmq_msg_send(&msg_payload, pub_, flags);
  bool ok = (rc != -1);
  
  if (ok) {
    messages_sent_.fetch_add(1);
  } else {
    int err = zmq_errno();
    if (err == EAGAIN) {
      // Buffer full - message dropped (non-blocking mode)
      messages_dropped_.fetch_add(1);
      LOG_WARN_COMP("ZmqPublisher", "Send buffer full - message dropped for topic: " + topic + 
                    " size: " + std::to_string(size) + " bytes");
    } else {
      LOG_ERROR_COMP("ZmqPublisher", "Failed to send message: " + std::string(zmq_strerror(err)));
    }
  }
  
  zmq_msg_close(&msg_payload);
  return ok;
}

bool ZmqPublisher::send_string(const std::string& topic, const std::string& payload, int flags) {
  LOG_DEBUG_COMP("ZmqPublisher", "Publishing to topic: " + topic + " payload size: " + std::to_string(payload.size()) + " bytes");
  bool result = send(topic, payload.data(), payload.size(), flags);
  // Logging is now handled in send() method with appropriate levels
  return result;
}

bool ZmqPublisher::is_buffer_available() const {
  if (!pub_ || !bound_) return false;
  
  // Check if socket is ready to send (not blocked)
  int events = 0;
  size_t events_size = sizeof(events);
  if (zmq_getsockopt(pub_, ZMQ_EVENTS, &events, &events_size) == 0) {
    // ZMQ_POLLOUT indicates socket can send without blocking
    return (events & ZMQ_POLLOUT) != 0;
  }
  return false;
}
