#include "zmq_subscriber.hpp"
#include <zmq.h>
#include <cstring>
#include <iostream>

ZmqSubscriber::ZmqSubscriber(const std::string& endpoint, const std::string& topic)
    : topic_(topic) {
  ctx_ = zmq_ctx_new();
  sub_ = zmq_socket(ctx_, ZMQ_SUB);
  zmq_setsockopt(sub_, ZMQ_SUBSCRIBE, topic_.data(), topic_.size());
  if (zmq_connect(sub_, endpoint.c_str()) != 0) {
    std::cerr << "[ZmqSubscriber] ZMQ connect failed to " << endpoint << ": " << zmq_strerror(zmq_errno()) << std::endl;
  } else {
    std::cout << "[ZmqSubscriber] Successfully connected to: " << endpoint << " topic: " << topic << std::endl;
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
  std::cout << "[ZmqSubscriber] Received message - topic: " << std::string(static_cast<char*>(zmq_msg_data(&topic)), zmq_msg_size(&topic))
            << " payload size: " << payload.size() << " bytes" << std::endl;
  zmq_msg_close(&topic);
  zmq_msg_close(&msg);
  return payload;
}

std::optional<DeltaMsg> ZmqSubscriber::parse_minimal_delta(const std::string& json) {
  // Extremely simple parse (no dependency): look for keys
  auto find_value = [&](const std::string& key) -> std::optional<std::string> {
    auto kpos = json.find("\"" + key + "\"");
    if (kpos == std::string::npos) return std::nullopt;
    auto cpos = json.find(':', kpos);
    if (cpos == std::string::npos) return std::nullopt;
    auto start = json.find_first_not_of(" \"", cpos + 1);
    auto end = json.find_first_of(",}\n\r\t\"", start);
    if (start == std::string::npos) return std::nullopt;
    if (end == std::string::npos) end = json.size();
    return json.substr(start, end - start);
  };

  auto at = find_value("asset_token");
  auto as = find_value("asset_symbol");
  auto du = find_value("delta_units");
  if (!at || !as || !du) return std::nullopt;
  DeltaMsg msg;
  msg.asset_token = at.value();
  // strip quotes
  if (!msg.asset_token.empty() && msg.asset_token.front()=='\"') msg.asset_token.erase(0,1);
  if (!msg.asset_token.empty() && msg.asset_token.back()=='\"') msg.asset_token.pop_back();
  msg.asset_symbol = as.value();
  if (!msg.asset_symbol.empty() && msg.asset_symbol.front()=='\"') msg.asset_symbol.erase(0,1);
  if (!msg.asset_symbol.empty() && msg.asset_symbol.back()=='\"') msg.asset_symbol.pop_back();
  try {
    msg.delta_units = std::stod(du.value());
  } catch (...) {
    return std::nullopt;
  }
  return msg;
}


