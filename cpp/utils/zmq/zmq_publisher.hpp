#pragma once
#include <string>
#include <memory>
#include <zmq.h>

class ZmqPublisher {
public:
  ZmqPublisher(const std::string& bind_endpoint, int hwm = 1000);
  ~ZmqPublisher();

  // Non-copyable
  ZmqPublisher(const ZmqPublisher&) = delete;
  ZmqPublisher& operator=(const ZmqPublisher&) = delete;

  bool bind();
  bool send(const std::string& topic, const void* data, size_t size, int flags = 0);
  bool send_string(const std::string& topic, const std::string& payload, int flags = 0);
  // Backward-compatible API
  bool publish(const std::string& topic, const std::string& payload) { return send_string(topic, payload, 0); }

private:
  void* ctx_;
  void* pub_;
  std::string endpoint_;
  int hwm_;
  bool bound_;
};
