#include "libuv_websocket_handler.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

LibuvWebSocketHandler::LibuvWebSocketHandler() {
    std::cout << "[LIBUV-WS] Initializing LibuvWebSocketHandler" << std::endl;
}

LibuvWebSocketHandler::~LibuvWebSocketHandler() {
    shutdown();
}

bool LibuvWebSocketHandler::initialize() {
    std::cout << "[LIBUV-WS] Initializing WebSocket handler" << std::endl;
    
    // Create libuv loop
    loop_ = uv_loop_new();
    if (!loop_) {
        std::cerr << "[LIBUV-WS] Failed to create libuv loop" << std::endl;
        return false;
    }
    
    // Initialize WebSocket data
    ws_data_ = std::make_unique<WebSocketData>();
    ws_data_->handler = this;
    
    running_.store(true);
    return true;
}

void LibuvWebSocketHandler::shutdown() {
    std::cout << "[LIBUV-WS] Shutting down WebSocket handler" << std::endl;
    
    running_.store(false);
    
    // Stop event loop
    stop_event_loop();
    
    // Clean up libuv resources
    if (loop_) {
        uv_loop_close(loop_);
        loop_ = nullptr;
    }
    
    ws_data_.reset();
    update_state(WebSocketState::DISCONNECTED);
}

bool LibuvWebSocketHandler::connect(const std::string& url) {
    if (!running_.load()) {
        std::cerr << "[LIBUV-WS] Handler not initialized" << std::endl;
        return false;
    }
    
    url_ = url;
    update_state(WebSocketState::CONNECTING);
    
    std::cout << "[LIBUV-WS] Connecting to: " << url_ << std::endl;
    
    // Parse URL
    std::string host, path;
    int port;
    bool ssl;
    
    if (!parse_url(url_, host, port, path, ssl)) {
        std::cerr << "[LIBUV-WS] Failed to parse URL: " << url_ << std::endl;
        update_state(WebSocketState::ERROR);
        return false;
    }
    
    // Initialize TCP connection
    uv_tcp_init(loop_, &ws_data_->tcp);
    
    // Set up connection request
    ws_data_->connect_req.data = ws_data_.get();
    
    // Resolve hostname and connect
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo* res;
    int err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
    if (err != 0) {
        std::cerr << "[LIBUV-WS] Failed to resolve hostname: " << host << std::endl;
        update_state(WebSocketState::ERROR);
        return false;
    }
    
    // Connect to the first available address
    err = uv_tcp_connect(&ws_data_->connect_req, &ws_data_->tcp, 
                        (const struct sockaddr*)res->ai_addr, on_connect);
    freeaddrinfo(res);
    
    if (err != 0) {
        std::cerr << "[LIBUV-WS] Failed to initiate connection: " << uv_strerror(err) << std::endl;
        update_state(WebSocketState::ERROR);
        return false;
    }
    
    // Start event loop in separate thread
    start_event_loop();
    
    return true;
}

void LibuvWebSocketHandler::disconnect() {
    std::cout << "[LIBUV-WS] Disconnecting WebSocket" << std::endl;
    
    update_state(WebSocketState::DISCONNECTING);
    
    if (ws_data_ && ws_data_->tcp.loop) {
        uv_close((uv_handle_t*)&ws_data_->tcp, on_close);
    }
    
    // Stop timers
    if (ws_data_) {
        uv_timer_stop(&ws_data_->ping_timer);
        uv_timer_stop(&ws_data_->reconnect_timer);
    }
}

bool LibuvWebSocketHandler::send_message(const std::string& message, bool binary) {
    if (state_ != WebSocketState::CONNECTED) {
        std::cerr << "[LIBUV-WS] Cannot send message, not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Create WebSocket frame
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = frame.rsv2 = frame.rsv3 = false;
    frame.opcode = binary ? WebSocketFrame::OPCODE_BINARY : WebSocketFrame::OPCODE_TEXT;
    frame.mask = true; // Client must mask frames
    frame.payload_length = message.length();
    frame.masking_key.resize(4);
    for (int i = 0; i < 4; i++) {
        frame.masking_key[i] = rand() & 0xFF;
    }
    frame.payload.assign(message.begin(), message.end());
    
    // Apply masking
    for (size_t i = 0; i < frame.payload.size(); ++i) {
        frame.payload[i] ^= ((uint8_t*)&frame.masking_key)[i % 4];
    }
    
    // Serialize frame
    std::vector<uint8_t> frame_data = serialize_frame(frame);
    
    // Send via libuv
    uv_buf_t buf = uv_buf_init((char*)frame_data.data(), frame_data.size());
    int err = uv_write(&ws_data_->write_req, (uv_stream_t*)&ws_data_->tcp, &buf, 1, on_write);
    
    if (err != 0) {
        std::cerr << "[LIBUV-WS] Failed to send message: " << uv_strerror(err) << std::endl;
        return false;
    }
    
    return true;
}

bool LibuvWebSocketHandler::send_binary(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::CONNECTED) {
        std::cerr << "[LIBUV-WS] Cannot send binary data, not connected" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Create WebSocket frame
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = frame.rsv2 = frame.rsv3 = false;
    frame.opcode = WebSocketFrame::OPCODE_BINARY;
    frame.mask = true;
    frame.payload_length = data.size();
    frame.masking_key.resize(4);
    for (int i = 0; i < 4; i++) {
        frame.masking_key[i] = rand() & 0xFF;
    }
    frame.payload = data;
    
    // Apply masking
    for (size_t i = 0; i < frame.payload.size(); ++i) {
        frame.payload[i] ^= ((uint8_t*)&frame.masking_key)[i % 4];
    }
    
    // Serialize and send
    std::vector<uint8_t> frame_data = serialize_frame(frame);
    uv_buf_t buf = uv_buf_init((char*)frame_data.data(), frame_data.size());
    int err = uv_write(&ws_data_->write_req, (uv_stream_t*)&ws_data_->tcp, &buf, 1, on_write);
    
    if (err != 0) {
        std::cerr << "[LIBUV-WS] Failed to send binary data: " << uv_strerror(err) << std::endl;
        return false;
    }
    
    return true;
}

// Static callback implementations
void LibuvWebSocketHandler::on_connect(uv_connect_t* req, int status) {
    WebSocketData* data = static_cast<WebSocketData*>(req->data);
    LibuvWebSocketHandler* handler = data->handler;
    
    if (status < 0) {
        std::cerr << "[LIBUV-WS] Connection failed: " << uv_strerror(status) << std::endl;
        handler->update_state(WebSocketState::ERROR);
        if (handler->error_callback_) {
            handler->error_callback_("Connection failed: " + std::string(uv_strerror(status)));
        }
        return;
    }
    
    std::cout << "[LIBUV-WS] TCP connection established" << std::endl;
    
    // Start reading
    int err = uv_read_start((uv_stream_t*)&data->tcp, alloc_buffer, on_read);
    if (err != 0) {
        std::cerr << "[LIBUV-WS] Failed to start reading: " << uv_strerror(err) << std::endl;
        handler->update_state(WebSocketState::ERROR);
        return;
    }
    
    // Perform WebSocket handshake
    if (!handler->perform_websocket_handshake()) {
        std::cerr << "[LIBUV-WS] WebSocket handshake failed" << std::endl;
        handler->update_state(WebSocketState::ERROR);
        return;
    }
    
    handler->update_state(WebSocketState::CONNECTED);
    if (handler->connect_callback_) {
        handler->connect_callback_(true);
    }
    
    // Start ping timer
    uv_timer_init(handler->loop_, &data->ping_timer);
    data->ping_timer.data = data;
    uv_timer_start(&data->ping_timer, on_ping_timer, 
                   handler->ping_interval_seconds_ * 1000, 
                   handler->ping_interval_seconds_ * 1000);
}

void LibuvWebSocketHandler::on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    WebSocketData* data = static_cast<WebSocketData*>(stream->data);
    LibuvWebSocketHandler* handler = data->handler;
    
    if (nread < 0) {
        if (nread != UV_EOF) {
            std::cerr << "[LIBUV-WS] Read error: " << uv_strerror(nread) << std::endl;
            handler->update_state(WebSocketState::ERROR);
            if (handler->error_callback_) {
                handler->error_callback_("Read error: " + std::string(uv_strerror(nread)));
            }
        }
        return;
    }
    
    if (nread > 0) {
        // Process received data
        std::string received_data(buf->base, nread);
        handler->process_received_data(received_data);
    }
    
    if (buf->base) {
        free(buf->base);
    }
}

void LibuvWebSocketHandler::on_write(uv_write_t* req, int status) {
    if (status < 0) {
        std::cerr << "[LIBUV-WS] Write error: " << uv_strerror(status) << std::endl;
        WebSocketData* data = static_cast<WebSocketData*>(req->data);
        LibuvWebSocketHandler* handler = data->handler;
        handler->update_state(WebSocketState::ERROR);
        if (handler->error_callback_) {
            handler->error_callback_("Write error: " + std::string(uv_strerror(status)));
        }
    }
}

void LibuvWebSocketHandler::on_ping_timer(uv_timer_t* timer) {
    WebSocketData* data = static_cast<WebSocketData*>(timer->data);
    LibuvWebSocketHandler* handler = data->handler;
    
    if (handler->state_ == WebSocketState::CONNECTED) {
        handler->send_ping();
    }
}

void LibuvWebSocketHandler::on_reconnect_timer(uv_timer_t* timer) {
    WebSocketData* data = static_cast<WebSocketData*>(timer->data);
    LibuvWebSocketHandler* handler = data->handler;
    
    handler->attempt_reconnect();
}

void LibuvWebSocketHandler::on_close(uv_handle_t* handle) {
    WebSocketData* data = static_cast<WebSocketData*>(handle->data);
    LibuvWebSocketHandler* handler = data->handler;
    
    std::cout << "[LIBUV-WS] Connection closed" << std::endl;
    handler->update_state(WebSocketState::DISCONNECTED);
    
    if (handler->connect_callback_) {
        handler->connect_callback_(false);
    }
    
    // Attempt reconnection if configured
    if (handler->running_.load() && 
        handler->reconnect_count_.load() < handler->reconnect_attempts_) {
        handler->attempt_reconnect();
    }
}

void LibuvWebSocketHandler::alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = static_cast<char*>(malloc(suggested_size));
    buf->len = suggested_size;
}

// Internal method implementations
void LibuvWebSocketHandler::start_event_loop() {
    if (event_loop_thread_.joinable()) {
        return; // Already running
    }
    
    event_loop_thread_ = std::thread([this]() {
        std::cout << "[LIBUV-WS] Starting event loop thread" << std::endl;
        uv_run(loop_, UV_RUN_DEFAULT);
        std::cout << "[LIBUV-WS] Event loop thread finished" << std::endl;
    });
}

void LibuvWebSocketHandler::stop_event_loop() {
    if (loop_) {
        uv_stop(loop_);
    }
    
    if (event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }
}

void LibuvWebSocketHandler::update_state(WebSocketState new_state) {
    WebSocketState old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        std::cout << "[LIBUV-WS] State changed from " << static_cast<int>(old_state) 
                  << " to " << static_cast<int>(new_state) << std::endl;
    }
}

void LibuvWebSocketHandler::send_ping() {
    if (state_ != WebSocketState::CONNECTED) {
        return;
    }
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = frame.rsv2 = frame.rsv3 = false;
    frame.opcode = WebSocketFrame::OPCODE_PING;
    frame.mask = true;
    frame.payload_length = 0;
    frame.masking_key.resize(4);
    for (int i = 0; i < 4; i++) {
        frame.masking_key[i] = rand() & 0xFF;
    }
    
    std::vector<uint8_t> frame_data = serialize_frame(frame);
    uv_buf_t buf = uv_buf_init((char*)frame_data.data(), frame_data.size());
    uv_write(&ws_data_->write_req, (uv_stream_t*)&ws_data_->tcp, &buf, 1, on_write);
}

void LibuvWebSocketHandler::attempt_reconnect() {
    int current_attempts = reconnect_count_.fetch_add(1);
    
    if (current_attempts >= reconnect_attempts_) {
        std::cerr << "[LIBUV-WS] Max reconnection attempts reached" << std::endl;
        update_state(WebSocketState::ERROR);
        return;
    }
    
    std::cout << "[LIBUV-WS] Attempting reconnection " << (current_attempts + 1) 
              << "/" << reconnect_attempts_ << std::endl;
    
    // Schedule reconnection
    uv_timer_init(loop_, &ws_data_->reconnect_timer);
    ws_data_->reconnect_timer.data = ws_data_.get();
    uv_timer_start(&ws_data_->reconnect_timer, on_reconnect_timer, 
                   reconnect_delay_seconds_ * 1000, 0);
}

bool LibuvWebSocketHandler::parse_url(const std::string& url, std::string& host, 
                                     int& port, std::string& path, bool& ssl) {
    // Simple URL parser for WebSocket URLs
    // Format: ws://host:port/path or wss://host:port/path
    
    std::string remaining_url = url;
    
    if (url.substr(0, 5) == "ws://") {
        ssl = false;
        remaining_url = url.substr(5);
    } else if (url.substr(0, 6) == "wss://") {
        ssl = true;
        remaining_url = url.substr(6);
    } else {
        return false;
    }
    
    size_t slash_pos = remaining_url.find('/');
    if (slash_pos != std::string::npos) {
        path = remaining_url.substr(slash_pos);
        remaining_url = remaining_url.substr(0, slash_pos);
    } else {
        path = "/";
    }
    
    size_t colon_pos = remaining_url.find(':');
    if (colon_pos != std::string::npos) {
        host = remaining_url.substr(0, colon_pos);
        port = std::stoi(remaining_url.substr(colon_pos + 1));
    } else {
        host = remaining_url;
        port = ssl ? 443 : 80;
    }
    
    return true;
}

bool LibuvWebSocketHandler::perform_websocket_handshake() {
    // Generate WebSocket key
    std::string key = generate_websocket_key();
    
    // Create handshake request
    std::ostringstream request;
    request << "GET " << path_ << " HTTP/1.1\r\n";
    request << "Host: " << host_ << ":" << port_ << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << key << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    request << "\r\n";
    
    std::string handshake = request.str();
    
    // Send handshake
    uv_buf_t buf = uv_buf_init((char*)handshake.c_str(), handshake.length());
    int err = uv_write(&ws_data_->write_req, (uv_stream_t*)&ws_data_->tcp, &buf, 1, on_write);
    
    return err == 0;
}

std::string LibuvWebSocketHandler::generate_websocket_key() {
    // Generate random 16-byte key and base64 encode it
    std::vector<uint8_t> key_bytes(16);
    for (int i = 0; i < 16; ++i) {
        key_bytes[i] = rand() % 256;
    }
    
    return base64_encode(key_bytes);
}

std::string LibuvWebSocketHandler::base64_encode(const std::vector<uint8_t>& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    
    BIO_write(bio, data.data(), data.size());
    BIO_flush(bio);
    
    BUF_MEM* buffer_ptr;
    BIO_get_mem_ptr(bio, &buffer_ptr);
    
    std::string result(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);
    
    return result;
}

void LibuvWebSocketHandler::process_received_data(const std::string& data) {
    // Simple WebSocket frame parsing
    if (data.length() < 2) {
        return;
    }
    
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.c_str());
    
    bool fin = (bytes[0] & 0x80) != 0;
    uint8_t opcode = bytes[0] & 0x0F;
    bool mask = (bytes[1] & 0x80) != 0;
    uint64_t payload_length = bytes[1] & 0x7F;
    
    size_t header_size = 2;
    if (payload_length == 126) {
        if (data.length() < 4) return;
        payload_length = (bytes[2] << 8) | bytes[3];
        header_size = 4;
    } else if (payload_length == 127) {
        if (data.length() < 10) return;
        payload_length = 0;
        for (int i = 0; i < 8; ++i) {
            payload_length = (payload_length << 8) | bytes[2 + i];
        }
        header_size = 10;
    }
    
    if (mask) {
        header_size += 4; // masking key
    }
    
    if (data.length() < header_size + payload_length) {
        return; // Incomplete frame
    }
    
    // Extract payload
    std::string payload = data.substr(header_size, payload_length);
    
    // Handle different frame types
    switch (opcode) {
        case WebSocketFrame::OPCODE_TEXT:
        case WebSocketFrame::OPCODE_BINARY: {
            WebSocketMessage msg;
            msg.data = payload;
            msg.is_binary = (opcode == WebSocketFrame::OPCODE_BINARY);
            msg.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            if (message_callback_) {
                message_callback_(msg);
            }
            break;
        }
        case WebSocketFrame::OPCODE_PING:
            // Send pong
            send_pong(payload);
            break;
        case WebSocketFrame::OPCODE_PONG:
            // Pong received, connection is alive
            break;
        case WebSocketFrame::OPCODE_CLOSE:
            // Close connection
            disconnect();
            break;
    }
}

void LibuvWebSocketHandler::send_pong(const std::string& payload) {
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = frame.rsv2 = frame.rsv3 = false;
    frame.opcode = WebSocketFrame::OPCODE_PONG;
    frame.mask = true;
    frame.payload_length = payload.length();
    frame.masking_key.resize(4);
    for (int i = 0; i < 4; i++) {
        frame.masking_key[i] = rand() & 0xFF;
    }
    frame.payload.assign(payload.begin(), payload.end());
    
    // Apply masking
    for (size_t i = 0; i < frame.payload.size(); ++i) {
        frame.payload[i] ^= ((uint8_t*)&frame.masking_key)[i % 4];
    }
    
    std::vector<uint8_t> frame_data = serialize_frame(frame);
    uv_buf_t buf = uv_buf_init((char*)frame_data.data(), frame_data.size());
    uv_write(&ws_data_->write_req, (uv_stream_t*)&ws_data_->tcp, &buf, 1, on_write);
}

std::vector<uint8_t> LibuvWebSocketHandler::serialize_frame(const WebSocketFrame& frame) {
    std::vector<uint8_t> result;
    
    // First byte
    uint8_t first_byte = 0;
    if (frame.fin) first_byte |= 0x80;
    if (frame.rsv1) first_byte |= 0x40;
    if (frame.rsv2) first_byte |= 0x20;
    if (frame.rsv3) first_byte |= 0x10;
    first_byte |= (frame.opcode & 0x0F);
    result.push_back(first_byte);
    
    // Second byte
    uint8_t second_byte = 0;
    if (frame.mask) second_byte |= 0x80;
    
    if (frame.payload_length < 126) {
        second_byte |= frame.payload_length;
        result.push_back(second_byte);
    } else if (frame.payload_length < 65536) {
        second_byte |= 126;
        result.push_back(second_byte);
        result.push_back((frame.payload_length >> 8) & 0xFF);
        result.push_back(frame.payload_length & 0xFF);
    } else {
        second_byte |= 127;
        result.push_back(second_byte);
        for (int i = 7; i >= 0; --i) {
            result.push_back((frame.payload_length >> (i * 8)) & 0xFF);
        }
    }
    
    // Masking key
    if (frame.mask) {
        for (size_t i = 0; i < frame.masking_key.size() && i < 4; i++) {
            result.push_back(frame.masking_key[i]);
        }
    }
    
    // Payload
    result.insert(result.end(), frame.payload.begin(), frame.payload.end());
    
    return result;
}
