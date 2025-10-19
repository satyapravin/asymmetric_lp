#pragma once
#include "i_http_handler.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <map>
#include <memory>
#include <thread>

#ifdef CURL_FOUND
#include <curl/curl.h>
#else
// Forward declaration for when CURL is not available
struct CURL;
#endif

// CURL-based HTTP handler implementation
class CurlHttpHandler : public IHttpHandler {
public:
    CurlHttpHandler();
    ~CurlHttpHandler() override;
    
    HttpResponse make_request(const HttpRequest& request) override;
    void make_request_async(const HttpRequest& request, HttpResponseCallback callback) override;
    
    bool initialize() override;
    void shutdown() override;
    bool is_initialized() const override { return initialized_; }
    
    void set_default_timeout(int timeout_ms) override { default_timeout_ms_ = timeout_ms; }
    void set_default_headers(const std::map<std::string, std::string>& headers) override { default_headers_ = headers; }
    void set_verify_ssl(bool verify) override { verify_ssl_ = verify; }

private:
    struct WriteCallbackData {
        std::string* buffer;
        HttpResponse* response;
    };
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* data);
    static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* data);
    
    void setup_curl_options(CURL* curl, const HttpRequest& request, WriteCallbackData& data);
    void add_headers_to_curl(CURL* curl, const std::map<std::string, std::string>& headers);
    
    bool initialized_{false};
    int default_timeout_ms_{5000};
    std::map<std::string, std::string> default_headers_;
    bool verify_ssl_{true};
    
#ifdef CURL_FOUND
    CURL* curl_{nullptr};
#endif
};

// Factory implementation
std::unique_ptr<IHttpHandler> HttpHandlerFactory::create(HttpHandlerFactory::Type type) {
    switch (type) {
        case HttpHandlerFactory::Type::CURL:
#ifdef CURL_FOUND
            return std::make_unique<CurlHttpHandler>();
#else
            throw std::runtime_error("CURL not available - cannot create CURL HTTP handler");
#endif
        case HttpHandlerFactory::Type::LIBUV:
            throw std::runtime_error("LIBUV HTTP handler not implemented yet");
        case HttpHandlerFactory::Type::CUSTOM:
            throw std::runtime_error("CUSTOM HTTP handler not implemented yet");
        default:
            throw std::runtime_error("Unknown HTTP handler type");
    }
}

std::unique_ptr<IHttpHandler> HttpHandlerFactory::create(const std::string& type_name) {
    if (type_name == "CURL" || type_name == "curl") {
        return create(Type::CURL);
    } else if (type_name == "LIBUV" || type_name == "libuv") {
        return create(Type::LIBUV);
    } else if (type_name == "CUSTOM" || type_name == "custom") {
        return create(Type::CUSTOM);
    } else {
        throw std::runtime_error("Unknown HTTP handler type: " + type_name);
    }
}
