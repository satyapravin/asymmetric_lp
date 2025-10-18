#pragma once
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <vector>

// HTTP request structure
struct HttpRequest {
    std::string method;           // GET, POST, PUT, DELETE
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    int timeout_ms{5000};
    bool verify_ssl{true};
};

// HTTP response structure
struct HttpResponse {
    int status_code{0};
    std::map<std::string, std::string> headers;
    std::string body;
    std::string error_message;
    bool success{false};
};

// HTTP response callback
using HttpResponseCallback = std::function<void(const HttpResponse& response)>;

// Base interface for HTTP handlers
class IHttpHandler {
public:
    virtual ~IHttpHandler() = default;
    
    // Synchronous HTTP request
    virtual HttpResponse make_request(const HttpRequest& request) = 0;
    
    // Asynchronous HTTP request
    virtual void make_request_async(const HttpRequest& request, HttpResponseCallback callback) = 0;
    
    // Lifecycle management
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_initialized() const = 0;
    
    // Configuration
    virtual void set_default_timeout(int timeout_ms) = 0;
    virtual void set_default_headers(const std::map<std::string, std::string>& headers) = 0;
    virtual void set_verify_ssl(bool verify) = 0;
};

// HTTP handler factory
class HttpHandlerFactory {
public:
    enum class Type {
        CURL,
        LIBUV,
        CUSTOM
    };
    
    static std::unique_ptr<IHttpHandler> create(Type type = Type::CURL);
    static std::unique_ptr<IHttpHandler> create(const std::string& type_name);
};
