#include "binance_private_websocket_handler.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace binance {

// CURL global state management with reference counting
namespace {
    std::mutex curl_init_mutex;
    std::atomic<int> curl_ref_count{0};
    
    void ensure_curl_initialized() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_add(1) == 0) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
    }
    
    void ensure_curl_cleanup() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_sub(1) == 1) {
            curl_global_cleanup();
        }
    }
}

// HTTP response callback for CURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch (std::bad_alloc& e) {
        return 0;
    }
}

BinancePrivateWebSocketHandler::BinancePrivateWebSocketHandler(const std::string& api_key, const std::string& api_secret)
    : api_key_(api_key), api_secret_(api_secret) {
    std::cout << "[BINANCE] Initializing Binance Private WebSocket Handler" << std::endl;
    
    // Initialize CURL with reference counting
    ensure_curl_initialized();
    
    // Create listen key
    listen_key_ = create_listen_key();
}

BinancePrivateWebSocketHandler::~BinancePrivateWebSocketHandler() {
    std::cout << "[BINANCE] Destroying Binance Private WebSocket Handler" << std::endl;
    disconnect();
    // Cleanup CURL with reference counting
    ensure_curl_cleanup();
}

bool BinancePrivateWebSocketHandler::connect(const std::string& url) {
    std::cout << "[BINANCE] Connecting to private WebSocket: " << url << std::endl;
    
    if (!is_authenticated()) {
        std::cerr << "[BINANCE] Cannot connect: not authenticated" << std::endl;
        return false;
    }
    
    websocket_url_ = url;
    state_.store(WebSocketState::CONNECTING);
    
    // Start connection thread
    connection_thread_running_.store(true);
    connection_thread_ = std::thread(&BinancePrivateWebSocketHandler::connection_loop, this);
    
    // Start listen key refresh thread
    refresh_thread_running_.store(true);
    listen_key_refresh_thread_ = std::thread(&BinancePrivateWebSocketHandler::listen_key_refresh_loop, this);
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (connected_.load()) {
        state_.store(WebSocketState::CONNECTED);
        if (connect_callback_) {
            connect_callback_(true);
        }
        std::cout << "[BINANCE] Connected successfully with listen key: " << listen_key_ << std::endl;
        return true;
    } else {
        state_.store(WebSocketState::ERROR);
        if (connect_callback_) {
            connect_callback_(false);
        }
        std::cerr << "[BINANCE] Failed to connect" << std::endl;
        return false;
    }
}

void BinancePrivateWebSocketHandler::disconnect() {
    std::cout << "[BINANCE] Disconnecting from private WebSocket" << std::endl;
    
    connected_.store(false);
    state_.store(WebSocketState::DISCONNECTING);
    should_stop_.store(true);
    
    // Stop connection thread
    connection_thread_running_.store(false);
    if (connection_thread_.joinable()) {
        connection_thread_.join();
    }
    
    // Stop listen key refresh thread
    refresh_thread_running_.store(false);
    if (listen_key_refresh_thread_.joinable()) {
        listen_key_refresh_thread_.join();
    }
    
    state_.store(WebSocketState::DISCONNECTED);
    std::cout << "[BINANCE] Disconnected" << std::endl;
}

bool BinancePrivateWebSocketHandler::is_connected() const {
    return connected_.load();
}

WebSocketState BinancePrivateWebSocketHandler::get_state() const {
    return state_.load();
}

bool BinancePrivateWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot send message: not connected" << std::endl;
        return false;
    }
    
    if (!is_authenticated()) {
        std::cerr << "[BINANCE] Cannot send message: not authenticated" << std::endl;
        return false;
    }
    
    std::cout << "[BINANCE] Sending message: " << message << std::endl;
    // In a real implementation, you would send the message via WebSocket
    return true;
}

bool BinancePrivateWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot send binary data: not connected" << std::endl;
        return false;
    }
    
    if (!is_authenticated()) {
        std::cerr << "[BINANCE] Cannot send binary data: not authenticated" << std::endl;
        return false;
    }
    
    std::cout << "[BINANCE] Sending binary data: " << data.size() << " bytes" << std::endl;
    // In a real implementation, you would send the binary data via WebSocket
    return true;
}

void BinancePrivateWebSocketHandler::set_message_callback(WebSocketMessageCallback callback) {
    message_callback_ = callback;
    std::cout << "[BINANCE] Message callback set" << std::endl;
}

void BinancePrivateWebSocketHandler::set_error_callback(WebSocketErrorCallback callback) {
    error_callback_ = callback;
    std::cout << "[BINANCE] Error callback set" << std::endl;
}

void BinancePrivateWebSocketHandler::set_connect_callback(WebSocketConnectCallback callback) {
    connect_callback_ = callback;
    std::cout << "[BINANCE] Connect callback set" << std::endl;
}

void BinancePrivateWebSocketHandler::set_ping_interval(int seconds) {
    ping_interval_.store(seconds);
    std::cout << "[BINANCE] Ping interval set to: " << seconds << " seconds" << std::endl;
}

void BinancePrivateWebSocketHandler::set_timeout(int seconds) {
    timeout_.store(seconds);
    std::cout << "[BINANCE] Timeout set to: " << seconds << " seconds" << std::endl;
}

void BinancePrivateWebSocketHandler::set_reconnect_attempts(int attempts) {
    reconnect_attempts_.store(attempts);
    std::cout << "[BINANCE] Reconnect attempts set to: " << attempts << std::endl;
}

void BinancePrivateWebSocketHandler::set_reconnect_delay(int seconds) {
    reconnect_delay_.store(seconds);
    std::cout << "[BINANCE] Reconnect delay set to: " << seconds << " seconds" << std::endl;
}

bool BinancePrivateWebSocketHandler::initialize() {
    std::cout << "[BINANCE] Initializing private WebSocket handler" << std::endl;
    return true;
}

void BinancePrivateWebSocketHandler::shutdown() {
    std::cout << "[BINANCE] Shutting down private WebSocket handler" << std::endl;
    disconnect();
}

bool BinancePrivateWebSocketHandler::subscribe_to_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot subscribe: not connected" << std::endl;
        return false;
    }
    
    if (!is_authenticated()) {
        std::cerr << "[BINANCE] Cannot subscribe: not authenticated" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    subscribed_channels_.push_back(channel);
    
    std::cout << "[BINANCE] Subscribed to channel: " << channel << std::endl;
    return true;
}

bool BinancePrivateWebSocketHandler::unsubscribe_from_channel(const std::string& channel) {
    if (!is_connected()) {
        std::cerr << "[BINANCE] Cannot unsubscribe: not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it);
        std::cout << "[BINANCE] Unsubscribed from channel: " << channel << std::endl;
        return true;
    }
    
    std::cerr << "[BINANCE] Channel not found: " << channel << std::endl;
    return false;
}

std::vector<std::string> BinancePrivateWebSocketHandler::get_subscribed_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    return subscribed_channels_;
}

void BinancePrivateWebSocketHandler::set_auth_credentials(const std::string& api_key, const std::string& secret) {
    api_key_ = api_key;
    api_secret_ = secret;
    std::cout << "[BINANCE] Authentication credentials updated" << std::endl;
}

bool BinancePrivateWebSocketHandler::is_authenticated() const {
    return !api_key_.empty() && !api_secret_.empty();
}

bool BinancePrivateWebSocketHandler::subscribe_to_user_data() {
    return subscribe_to_channel(listen_key_);
}

bool BinancePrivateWebSocketHandler::subscribe_to_order_updates() {
    return subscribe_to_channel("order_update");
}

bool BinancePrivateWebSocketHandler::subscribe_to_account_updates() {
    return subscribe_to_channel("account_update");
}

bool BinancePrivateWebSocketHandler::subscribe_to_balance_updates() {
    return subscribe_to_channel("balance_update");
}

bool BinancePrivateWebSocketHandler::subscribe_to_position_updates() {
    return subscribe_to_channel("position_update");
}

bool BinancePrivateWebSocketHandler::refresh_listen_key() {
    std::cout << "[BINANCE] Refreshing listen key" << std::endl;
    
    std::string new_key = create_listen_key();
    if (!new_key.empty()) {
        listen_key_ = new_key;
        std::cout << "[BINANCE] Listen key refreshed: " << listen_key_ << std::endl;
        return true;
    }
    
    return false;
}

void BinancePrivateWebSocketHandler::handle_message(const std::string& message) {
    handle_websocket_message(message);
}

void BinancePrivateWebSocketHandler::connection_loop() {
    std::cout << "[BINANCE] Starting connection loop" << std::endl;
    
    // Simulate connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    connected_.store(true);
    
    // Keep connection alive
    while (connection_thread_running_.load() && !should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(ping_interval_.load()));
        
        if (connection_thread_running_.load()) {
            // Send ping
            std::cout << "[BINANCE] Sending ping" << std::endl;
        }
    }
    
    connected_.store(false);
    std::cout << "[BINANCE] Connection loop stopped" << std::endl;
}

void BinancePrivateWebSocketHandler::listen_key_refresh_loop() {
    std::cout << "[BINANCE] Starting listen key refresh loop" << std::endl;
    
    while (refresh_thread_running_.load() && !should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(30)); // Refresh every 30 minutes
        
        if (refresh_thread_running_.load()) {
            keep_alive_listen_key();
        }
    }
    
    std::cout << "[BINANCE] Listen key refresh loop stopped" << std::endl;
}

void BinancePrivateWebSocketHandler::handle_websocket_message(const std::string& message) {
    if (message_callback_) {
        WebSocketMessage ws_message;
        ws_message.data = message;
        ws_message.is_binary = false;
        ws_message.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ws_message.channel = "private";
        
        message_callback_(ws_message);
    }
    
    std::cout << "[BINANCE] Received private message: " << message << std::endl;
}

std::string BinancePrivateWebSocketHandler::create_listen_key() {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[BINANCE] Failed to initialize CURL for listen key creation" << std::endl;
        return "";
    }
    
    std::string url = "https://fapi.binance.com/fapi/v1/listenKey";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);
    
    struct curl_slist* header_list = nullptr;
    header_list = curl_slist_append(header_list, ("X-MBX-APIKEY: " + api_key_).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[BINANCE] CURL error creating listen key: " << curl_easy_strerror(res) << std::endl;
        response.clear();
    }
    
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    
    // In a real implementation, you would parse the JSON response to extract the listen key
    // For now, return a mock listen key
    return "mock_listen_key_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

bool BinancePrivateWebSocketHandler::keep_alive_listen_key() {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[BINANCE] Failed to initialize CURL for listen key keep alive" << std::endl;
        return false;
    }
    
    std::string url = "https://fapi.binance.com/fapi/v1/listenKey";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);
    
    struct curl_slist* header_list = nullptr;
    header_list = curl_slist_append(header_list, ("X-MBX-APIKEY: " + api_key_).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[BINANCE] CURL error keeping alive listen key: " << curl_easy_strerror(res) << std::endl;
        response.clear();
    }
    
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    
    return !response.empty();
}

} // namespace binance