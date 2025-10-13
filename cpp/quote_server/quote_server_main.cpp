#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "../utils/config/config.hpp"
#include "../utils/zmq/zmq_publisher.hpp"
#include "../utils/mds/orderbook_binary.hpp"
#include "../utils/mds/market_data_normalizer.hpp"

static std::vector<char> build_mock_book_binary(const std::string& symbol, double mid) {
  std::vector<std::pair<double, double>> bids, asks;
  bids.emplace_back(mid * 0.999, 1.0);
  bids.emplace_back(mid * 0.998, 2.0);
  asks.emplace_back(mid * 1.001, 1.0);
  asks.emplace_back(mid * 1.002, 2.0);
  
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  size_t size = OrderBookBinaryHelper::calculate_size(symbol.length(), bids.size(), asks.size());
  std::vector<char> buffer(size);
  
  OrderBookBinaryHelper::serialize(symbol, bids, asks, now, 0, buffer.data(), size);
  return buffer;
}

int main() {
  auto cfg = load_app_config();
  std::string md_bind = cfg.md_pub_endpoint;
  ZmqPublisher pub(md_bind);
  std::cout << "Quote server publishing on " << md_bind << std::endl;

  std::string topic = cfg.md_topic.empty() ? (std::string("md.") + cfg.exchanges_csv + "." + cfg.symbol) : cfg.md_topic;
  double mid = 2000.0;
  while (true) {
    auto binary_data = build_mock_book_binary(cfg.symbol, mid);
    pub.publish(topic, std::string(binary_data.data(), binary_data.size()));
    mid *= 1.0 + 0.0001;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return 0;
}
