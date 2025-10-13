#pragma once
#include <string>

class ZmqPublisher {
public:
  ZmqPublisher(const std::string& bind_endpoint);
  ~ZmqPublisher();
  bool publish(const std::string& topic, const std::string& payload);
private:
  void* ctx_{};
  void* pub_{};
};
