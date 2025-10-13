#pragma once
#include <optional>
#include <string>

struct DeltaMsg {
  std::string asset_token;
  std::string asset_symbol;
  double delta_units{};
};

class ZmqSubscriber {
 public:
  ZmqSubscriber(const std::string& endpoint, const std::string& topic);
  std::optional<std::string> receive();
  static std::optional<DeltaMsg> parse_minimal_delta(const std::string& json);
 private:
  void* ctx_{};
  void* sub_{};
  std::string topic_;
};


