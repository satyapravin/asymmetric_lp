#pragma once
#include <optional>
#include <string>

class ZmqSubscriber {
 public:
  ZmqSubscriber(const std::string& endpoint, const std::string& topic);
  ~ZmqSubscriber();
  std::optional<std::string> receive();
  std::optional<std::string> receive_blocking(int timeout_ms = 1000);
 private:
  void* ctx_{};
  void* sub_{};
  std::string topic_;
};


