#include "curl_http_handler.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <cstring>

#ifdef CURL_FOUND
#include <curl/curl.h>
#endif

CurlHttpHandler::CurlHttpHandler() {
#ifdef CURL_FOUND
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL");
    }
#endif
}

CurlHttpHandler::~CurlHttpHandler() {
#ifdef CURL_FOUND
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
#endif
}

bool CurlHttpHandler::initialize() {
#ifdef CURL_FOUND
    if (!curl_) {
        curl_ = curl_easy_init();
        if (!curl_) {
            return false;
        }
    }
    
    // Set default options
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "AsymmetricLP/1.0");
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, default_timeout_ms_);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, verify_ssl_ ? 1L : 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, verify_ssl_ ? 2L : 0L);
    
    initialized_ = true;
    return true;
#else
    return false;
#endif
}

void CurlHttpHandler::shutdown() {
#ifdef CURL_FOUND
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
#endif
    initialized_ = false;
}

HttpResponse CurlHttpHandler::make_request(const HttpRequest& request) {
#ifdef CURL_FOUND
    if (!initialized_) {
        HttpResponse response;
        response.error_message = "HTTP handler not initialized";
        return response;
    }
    
    HttpResponse response;
    WriteCallbackData data;
    data.buffer = &response.body;
    data.response = &response;
    
    // Reset CURL handle
    curl_easy_reset(curl_);
    
    // Setup CURL options
    setup_curl_options(curl_, request, data);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        response.error_message = "CURL error: " + std::string(curl_easy_strerror(res));
        return response;
    }
    
    // Get response code
    long response_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    response.status_code = static_cast<int>(response_code);
    response.success = (response_code >= 200 && response_code < 300);
    
    return response;
#else
    HttpResponse response;
    response.error_message = "CURL not available";
    return response;
#endif
}

void CurlHttpHandler::make_request_async(const HttpRequest& request, HttpResponseCallback callback) {
    // For now, implement as synchronous call in a separate thread
    // In a production system, this would use CURL's multi interface or libuv
    std::thread([this, request, callback]() {
        HttpResponse response = make_request(request);
        callback(response);
    }).detach();
}

void CurlHttpHandler::setup_curl_options(CURL* curl, const HttpRequest& request, WriteCallbackData& data) {
#ifdef CURL_FOUND
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    
    // Set method
    if (request.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    } else if (request.method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    } else if (request.method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    
    // Set timeout
    int timeout = request.timeout_ms > 0 ? request.timeout_ms : default_timeout_ms_;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
    
    // Set SSL verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.verify_ssl ? 2L : 0L);
    
    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    
    // Set header callback
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &data);
    
    // Add headers
    add_headers_to_curl(curl, request.headers);
    
    // Add default headers
    add_headers_to_curl(curl, default_headers_);
#endif
}

void CurlHttpHandler::add_headers_to_curl(CURL* curl, const std::map<std::string, std::string>& headers) {
#ifdef CURL_FOUND
    if (headers.empty()) return;
    
    struct curl_slist* header_list = nullptr;
    
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
#endif
}

size_t CurlHttpHandler::WriteCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* data) {
    if (!data || !data->buffer) return 0;
    
    size_t total_size = size * nmemb;
    data->buffer->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t CurlHttpHandler::HeaderCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* data) {
    if (!data || !data->response) return 0;
    
    size_t total_size = size * nmemb;
    std::string header_line(static_cast<char*>(contents), total_size);
    
    // Remove trailing newline
    if (!header_line.empty() && header_line.back() == '\n') {
        header_line.pop_back();
    }
    if (!header_line.empty() && header_line.back() == '\r') {
        header_line.pop_back();
    }
    
    // Parse header (format: "Key: Value")
    size_t colon_pos = header_line.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header_line.substr(0, colon_pos);
        std::string value = header_line.substr(colon_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        data->response->headers[key] = value;
    }
    
    return total_size;
}