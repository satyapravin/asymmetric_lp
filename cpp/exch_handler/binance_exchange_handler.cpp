#include "binance_exchange_handler.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/sha.h>

BinanceExchangeHandler::BinanceExchangeHandler() {
  loop_ = uv_loop_new();
  base_url_ = testnet_mode_ ? "https://testnet.binance.vision" : "https://api.binance.com";
  ws_url_ = testnet_mode_ ? "wss://testnet.binance.vision" : "wss://stream.binance.com:9443";
}

BinanceExchangeHandler::~BinanceExchangeHandler() {
  stop();
  uv_loop_close(loop_);
}

bool BinanceExchangeHandler::start() {
  if (running_.load()) return true;
  
  running_.store(true);
  event_loop_thread_ = std::thread([this]() { this->run_event_loop(); });
  
  std::cout << "[BINANCE_HANDLER] Starting Binance exchange handler" << std::endl;
  
  return true;
}

void BinanceExchangeHandler::stop() {
  if (!running_.load()) return;
  
  running_.store(false);
  
  // Stop event loop
  uv_async_t stop_async;
  uv_async_init(loop_, &stop_async, [](uv_async_t* handle) {
    uv_stop(handle->loop);
  });
  uv_async_send(&stop_async);
  
  if (event_loop_thread_.joinable()) {
    event_loop_thread_.join();
  }
  
  std::cout << "[BINANCE_HANDLER] Stopped" << std::endl;
}

bool BinanceExchangeHandler::send_order(const Order& order) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  // Store order locally
  active_orders_[order.client_order_id] = order;
  
  // Send to Binance
  send_binance_order(order);
  
  std::cout << "[BINANCE_HANDLER] Sent order: " << order.client_order_id 
            << " " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
            << " " << order.quantity << " @ " << order.price << std::endl;
  
  return true;
}

bool BinanceExchangeHandler::cancel_order(const std::string& client_order_id) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = active_orders_.find(client_order_id);
  if (it != active_orders_.end()) {
    cancel_binance_order(client_order_id);
    std::cout << "[BINANCE_HANDLER] Cancelled order: " << client_order_id << std::endl;
    return true;
  }
  
  return false;
}

bool BinanceExchangeHandler::modify_order(const Order& order) {
  // Binance doesn't support order modification, need to cancel and resend
  if (cancel_order(order.client_order_id)) {
    return send_order(order);
  }
  return false;
}

std::vector<Order> BinanceExchangeHandler::get_open_orders() {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  std::vector<Order> open_orders;
  for (const auto& [id, order] : active_orders_) {
    if (order.status == OrderStatus::PENDING) {
      open_orders.push_back(order);
    }
  }
  
  return open_orders;
}

Order BinanceExchangeHandler::get_order_status(const std::string& client_order_id) {
  std::lock_guard<std::mutex> lock(orders_mutex_);
  
  auto it = active_orders_.find(client_order_id);
  if (it != active_orders_.end()) {
    return it->second;
  }
  
  return Order{}; // Return empty order if not found
}

void BinanceExchangeHandler::send_binance_order(const Order& order) {
  // Create Binance order request
  std::stringstream params;
  params << "symbol=" << order.symbol
         << "&side=" << (order.side == OrderSide::BUY ? "BUY" : "SELL")
         << "&type=" << (order.type == OrderType::MARKET ? "MARKET" : "LIMIT")
         << "&quantity=" << order.quantity;
  
  if (order.type == OrderType::LIMIT) {
    params << "&price=" << order.price
           << "&timeInForce=GTC";
  }
  
  params << "&newClientOrderId=" << order.client_order_id
         << "&timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
  
  std::string query_string = params.str();
  std::string signature = generate_signature(query_string);
  
  std::string body = query_string + "&signature=" + signature;
  std::string headers = create_auth_headers("POST", "/api/v3/order", body);
  
  make_http_request("POST", "/api/v3/order", body, headers);
}

void BinanceExchangeHandler::cancel_binance_order(const std::string& client_order_id) {
  std::stringstream params;
  params << "origClientOrderId=" << client_order_id
         << "&timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
  
  std::string query_string = params.str();
  std::string signature = generate_signature(query_string);
  
  std::string body = query_string + "&signature=" + signature;
  std::string headers = create_auth_headers("DELETE", "/api/v3/order", body);
  
  make_http_request("DELETE", "/api/v3/order", body, headers);
}

std::string BinanceExchangeHandler::generate_signature(const std::string& query_string) {
  // HMAC-SHA256 signature generation
  unsigned char* digest;
  unsigned int digest_len;
  
  digest = HMAC(EVP_sha256(), secret_key_.c_str(), secret_key_.length(),
                (unsigned char*)query_string.c_str(), query_string.length(),
                nullptr, &digest_len);
  
  // Convert to hex string
  std::stringstream ss;
  for (unsigned int i = 0; i < digest_len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  
  return ss.str();
}

std::string BinanceExchangeHandler::create_auth_headers(const std::string& method, 
                                                       const std::string& endpoint, 
                                                       const std::string& body) {
  std::stringstream headers;
  headers << "X-MBX-APIKEY: " << api_key_ << "\r\n";
  headers << "Content-Type: application/x-www-form-urlencoded\r\n";
  headers << "Content-Length: " << body.length() << "\r\n";
  
  return headers.str();
}

void BinanceExchangeHandler::make_http_request(const std::string& method, 
                                              const std::string& endpoint, 
                                              const std::string& body, 
                                              const std::string& headers) {
  // Simplified HTTP request - in production use a proper HTTP client
  std::cout << "[BINANCE_HANDLER] HTTP " << method << " " << endpoint << std::endl;
  std::cout << "[BINANCE_HANDLER] Body: " << body << std::endl;
  
  // Simulate response handling
  handle_binance_response(R"({"orderId":12345,"clientOrderId":")" + 
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + 
                         R"(","symbol":"ETHUSDT","status":"NEW"})");
}

void BinanceExchangeHandler::handle_binance_response(const std::string& response) {
  // Parse Binance response and update order status
  // In production, use proper JSON parsing
  
  std::cout << "[BINANCE_HANDLER] Received response: " << response << std::endl;
  
  // Simulate order status update
  std::lock_guard<std::mutex> lock(orders_mutex_);
  for (auto& [id, order] : active_orders_) {
    if (order.status == OrderStatus::PENDING) {
      order.status = OrderStatus::FILLED;
      order.filled_quantity = order.quantity;
      order.average_price = order.price;
      
      if (order_event_callback_) {
        order_event_callback_(order);
      }
      break;
    }
  }
}

void BinanceExchangeHandler::run_event_loop() {
  uv_run(loop_, UV_RUN_DEFAULT);
}

// Static libuv callbacks
void BinanceExchangeHandler::on_http_response(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  if (nread > 0) {
    std::string response(buf->base, nread);
    BinanceExchangeHandler* handler = static_cast<BinanceExchangeHandler*>(stream->data);
    handler->handle_binance_response(response);
  }
  
  if (buf->base) {
    free(buf->base);
  }
}

void BinanceExchangeHandler::on_http_connect(uv_connect_t* req, int status) {
  BinanceExchangeHandler* handler = static_cast<BinanceExchangeHandler*>(req->data);
  
  if (status < 0) {
    std::cerr << "[BINANCE_HANDLER] HTTP connection failed: " << uv_strerror(status) << std::endl;
    handler->connected_.store(false);
    return;
  }
  
  std::cout << "[BINANCE_HANDLER] HTTP connected" << std::endl;
  handler->connected_.store(true);
}
